#include "MaterialSystem.hpp"
#include "ecs/systems/RenderSystem.hpp"
#include "Renderer.hpp"
#include "Program.h"
#include "Storage/Hashmap.h"
#include "Storage/Slotmap.h"
#include "Engine.hpp"

using namespace BB;

BB_STATIC_ASSERT(sizeof(ShaderIndices) == sizeof(ShaderIndices2D), "shaderindices not the same sizeof.");

constexpr size_t PUSH_CONSTANT_SPACE_SIZE = sizeof(ShaderIndices);

static uint64_t ShaderEffectHash(const MaterialShaderCreateInfo& a_create_info)
{
	uint64_t hash = 256;
	for (size_t i = 0; i < a_create_info.entry.size() - 1; i++)
	{
		hash += static_cast<uint64_t>(a_create_info.entry.c_str()[i]);
	}

	const StringView path_no_extension = StringView(a_create_info.path.c_str(), a_create_info.path.find_last_of('.'));
	for (size_t i = path_no_extension.find_last_of('/'); i < path_no_extension.size(); i++)
	{
		hash *= static_cast<uint64_t>(path_no_extension.c_str()[i]);
	}

	hash *= static_cast<uint32_t>(a_create_info.stage);
	hash ^= a_create_info.next_stages;

	return hash;
}

struct MaterialSystem_inst
{
	StaticSlotmap<MasterMaterial, MasterMaterialHandle> material_map;
	StaticArray<CachedShaderInfo> shader_effects;
	StaticOL_HashMap<uint64_t, ShaderEffectHandle> shader_effect_cache;

	FreelistArray<MaterialInstance> material_instances;

	RDescriptorLayout scene_desc_layout;
	RDescriptorLayout material_desc_layout;
	DescriptorAllocation material_desc_allocation;

	MasterMaterialHandle default_materials[static_cast<uint32_t>(PASS_TYPE::ENUM_SIZE)][static_cast<uint32_t>(MATERIAL_TYPE::ENUM_SIZE)];

    PathString shader_path;
};

static MaterialSystem_inst* s_material_inst;

static inline MasterMaterialHandle& GetDefaultMasterMaterial_impl(const PASS_TYPE a_pass_type, const MATERIAL_TYPE a_material_type)
{
	return s_material_inst->default_materials[static_cast<uint32_t>(a_pass_type)][static_cast<uint32_t>(a_material_type)];
}

static inline MasterMaterialHandle CreateMaterial_impl(const ShaderEffectHandle a_vertex, const ShaderEffectHandle a_fragment, const ShaderEffectHandle a_geometry, const PASS_TYPE a_pass_type, const MATERIAL_TYPE a_material_type, const uint32_t a_user_data_size, const bool a_cpu_writeable, const StringView name = "default")
{
	const MasterMaterialHandle material = s_material_inst->material_map.emplace();
	MasterMaterial& inst = s_material_inst->material_map.find(material);
	inst.name = name;
	inst.shaders.vertex = a_vertex;
	inst.shaders.fragment_pixel = a_fragment;
	inst.shaders.geometry = a_geometry;
	inst.pass_type = a_pass_type;
	inst.material_type = a_material_type;
	inst.user_data_size = a_user_data_size;
	inst.handle = material;
	inst.cpu_writeable = a_cpu_writeable;
	return material;
}

struct ShaderEffectList
{
    ShaderEffectHandle vertex;
    ShaderEffectHandle fragment;
    ShaderEffectHandle geometry;
};

static ShaderEffectList CreateShaderEffects_impl(MemoryArena& a_temp_arena, const Slice<MaterialShaderCreateInfo> a_shader_effects_info, const ShaderDescriptorLayouts& a_desc_layouts, const uint32_t a_desc_layout_count)
{
    auto place_shader = [](ShaderEffectList& a_list, const SHADER_STAGE a_stage, const ShaderEffectHandle a_effect) -> bool
        {
            switch (a_stage)
            {
            case SHADER_STAGE::VERTEX:
                if (a_list.vertex.IsValid())
                    return false;
                a_list.vertex = a_effect;
                break;
            case SHADER_STAGE::FRAGMENT_PIXEL:
                if (a_list.fragment.IsValid())
                    return false;
                a_list.fragment = a_effect;
                break;
            case SHADER_STAGE::GEOMETRY:
                if (a_list.geometry.IsValid())
                    return false;
                a_list.geometry = a_effect;
                break;
            default:
                BB_ASSERT(false, "something went wrong in creating shaders");
                return false;
            }
            return true;
        };

    ShaderEffectList return_list;
	CreateShaderEffectInfo* shader_effects = ArenaAllocArr(a_temp_arena, CreateShaderEffectInfo, a_shader_effects_info.size());
	size_t created_shader_effect_count = 0;

	MemoryArenaMarker memory_pos_before_shader_read = MemoryArenaGetMemoryMarker(a_temp_arena);
	PathString previous_shader_path;
	Buffer shader_buffer;

	for (size_t i = 0; i < a_shader_effects_info.size(); i++)
	{
		const MaterialShaderCreateInfo& info = a_shader_effects_info[i];

		if (const ShaderEffectHandle* found_shader = s_material_inst->shader_effect_cache.find(ShaderEffectHash(info)))
		{
            if (!place_shader(return_list, info.stage, *found_shader))
                return ShaderEffectList();
		}
		else
		{
			if (previous_shader_path != info.path)
			{
				MemoryArenaSetMemoryMarker(a_temp_arena, memory_pos_before_shader_read);
                PathString path = s_material_inst->shader_path;
                path.AddPathNoSlash(info.path.c_str());
				shader_buffer = OSReadFile(a_temp_arena, path.c_str());
                previous_shader_path = path;
			}

			const StringView name = info.path.c_str() + info.path.find_last_of('/');

			CreateShaderEffectInfo& shader_info = shader_effects[created_shader_effect_count++];
			shader_info.name = name.c_str();
			shader_info.shader_entry = info.entry.c_str();
			shader_info.shader_data = shader_buffer;
			shader_info.stage = info.stage;
			shader_info.next_stages = info.next_stages;
			shader_info.push_constant_space = PUSH_CONSTANT_SPACE_SIZE;
			shader_info.desc_layouts = a_desc_layouts;
			shader_info.desc_layout_count = a_desc_layout_count;
		}
	}

	// all shaders already exist, return them.
	if (created_shader_effect_count == 0)
	{
		return return_list;
	}

	ShaderEffectHandle* created_handles = ArenaAllocArr(a_temp_arena, ShaderEffectHandle, created_shader_effect_count);
	bool success = CreateShaderEffect(a_temp_arena, Slice(shader_effects, created_shader_effect_count), created_handles, false);
	if (!success)
	{
		BB_WARNING(false, "Material created failed due to failing to create shader effects", WarningType::MEDIUM);
		return ShaderEffectList();
	}
	else
	{
		// all created shaders are filled, send them.
		if (a_shader_effects_info.size() == created_shader_effect_count)
		{
			for (size_t i = 0; i < created_shader_effect_count; i++)
			{
				CachedShaderInfo shader_info{};
				shader_info.handle = created_handles[i];
				shader_info.path.append(a_shader_effects_info[i].path);
				shader_info.entry.append(a_shader_effects_info[i].entry);
				shader_info.stage = a_shader_effects_info[i].stage;
				shader_info.next_stages = a_shader_effects_info[i].next_stages;
				s_material_inst->shader_effects.emplace_back(shader_info);
				s_material_inst->shader_effect_cache.insert(ShaderEffectHash(a_shader_effects_info[i]), shader_info.handle);

                if (!place_shader(return_list, shader_info.stage, created_handles[i]))
                    return ShaderEffectList();
			}


			return return_list;
		}

		// some are created and some are found. Sort them inside the empty slots in found_handles
		for (size_t i = 0; i < created_shader_effect_count; i++)
		{
            CachedShaderInfo shader_info{};
            shader_info.handle = created_handles[i];
            shader_info.path.append(a_shader_effects_info[i].path);
            shader_info.entry.append(a_shader_effects_info[i].entry);
            shader_info.stage = a_shader_effects_info[i].stage;
            shader_info.next_stages = a_shader_effects_info[i].next_stages;
            s_material_inst->shader_effects.emplace_back(shader_info);
            s_material_inst->shader_effect_cache.insert(ShaderEffectHash(a_shader_effects_info[i]), shader_info.handle);

            if (!place_shader(return_list, shader_info.stage, created_handles[i]))
                return ShaderEffectList();
		}

	}
	return return_list;
}

void Material::InitMaterialSystem(MemoryArena& a_arena, const MaterialSystemCreateInfo& a_create_info)
{
	BB_ASSERT(a_create_info.default_2d_vertex.stage == SHADER_STAGE::VERTEX, "default_2d_vertex.stage is not SHADER_STAGE::VERTEX");
	BB_ASSERT(a_create_info.default_2d_fragment.stage == SHADER_STAGE::FRAGMENT_PIXEL, "default_2d_fragment.stage is not SHADER_STAGE::FRAGMENT_PIXEL");
	BB_ASSERT(a_create_info.default_3d_vertex.stage == SHADER_STAGE::VERTEX, "default_3d_vertex.stage is not SHADER_STAGE::VERTEX");
	BB_ASSERT(a_create_info.default_3d_fragment.stage == SHADER_STAGE::FRAGMENT_PIXEL, "default_3d_fragment.stage is not SHADER_STAGE::FRAGMENT_PIXEL");

	s_material_inst = ArenaAllocType(a_arena, MaterialSystem_inst);
	s_material_inst->material_map.Init(a_arena, a_create_info.max_materials);
	s_material_inst->material_instances.Init(a_arena, a_create_info.max_material_instances);
	s_material_inst->shader_effects.Init(a_arena, a_create_info.max_shader_effects);
	s_material_inst->shader_effect_cache.Init(a_arena, a_create_info.max_shader_effects);
	
	s_material_inst->scene_desc_layout = RenderSystem::GetSceneDescriptorLayout();
    s_material_inst->shader_path = GetRootPath();
    s_material_inst->shader_path.AddPath("resources");
    s_material_inst->shader_path.AddPath("shaders");

	MemoryArenaScope(a_arena)
	{
		{
			DescriptorBindingInfo desc_binding;
			desc_binding.type = DESCRIPTOR_TYPE::READONLY_CONSTANT;
			desc_binding.count = a_create_info.max_material_instances;
			desc_binding.binding = PER_MATERIAL_BINDING;
			desc_binding.shader_stage = SHADER_STAGE::ALL;
			s_material_inst->material_desc_layout = CreateDescriptorLayout(a_arena, ConstSlice<DescriptorBindingInfo>(&desc_binding, 1));
			s_material_inst->material_desc_allocation = AllocateDescriptor(s_material_inst->material_desc_layout);
		}

		MaterialCreateInfo material_info;
		material_info.material_type = MATERIAL_TYPE::MATERIAL_2D;
		material_info.cpu_writeable = false;
		MaterialShaderCreateInfo default_2d_shaders[]{ a_create_info.default_2d_vertex, a_create_info.default_2d_fragment };
		material_info.shader_infos = Slice(default_2d_shaders, _countof(default_2d_shaders));
		material_info.user_data_size = 0;

		material_info.pass_type = PASS_TYPE::GLOBAL;
		GetDefaultMasterMaterial_impl(PASS_TYPE::GLOBAL, MATERIAL_TYPE::MATERIAL_2D) = CreateMasterMaterial(a_arena, material_info, "default global 2d");
		material_info.pass_type = PASS_TYPE::SCENE;
		GetDefaultMasterMaterial_impl(PASS_TYPE::SCENE, MATERIAL_TYPE::MATERIAL_2D) = CreateMasterMaterial(a_arena, material_info, "default scene 2d");

		MaterialShaderCreateInfo default_3d_shaders[]{ a_create_info.default_3d_vertex, a_create_info.default_3d_fragment };
		material_info.material_type = MATERIAL_TYPE::MATERIAL_3D;
		material_info.shader_infos = Slice(default_3d_shaders, _countof(default_3d_shaders));
		material_info.user_data_size = sizeof(MeshMetallic);

		material_info.pass_type = PASS_TYPE::GLOBAL;
		GetDefaultMasterMaterial_impl(PASS_TYPE::GLOBAL, MATERIAL_TYPE::MATERIAL_3D) = CreateMasterMaterial(a_arena, material_info, "default global 3d");
		material_info.pass_type = PASS_TYPE::SCENE;
		GetDefaultMasterMaterial_impl(PASS_TYPE::SCENE, MATERIAL_TYPE::MATERIAL_3D) = CreateMasterMaterial(a_arena, material_info, "default scene 3d");
	}
}

MasterMaterialHandle Material::CreateMasterMaterial(MemoryArena& a_temp_arena, const MaterialCreateInfo& a_create_info, const StringView a_name)
{
	ShaderDescriptorLayouts desc_layouts;
	desc_layouts[SPACE_IMMUTABLE_SAMPLER] = GetStaticSamplerDescriptorLayout();
	desc_layouts[SPACE_GLOBAL] = GetGlobalDescriptorLayout();
	desc_layouts[SPACE_PER_SCENE] = RDescriptorLayout(BB_INVALID_HANDLE_64);	// PER SCENE SET
	desc_layouts[SPACE_PER_MATERIAL] = RDescriptorLayout(BB_INVALID_HANDLE_64);	// PER MATERIAL SET
	desc_layouts[SPACE_PER_MESH] = RDescriptorLayout(BB_INVALID_HANDLE_64);	// PER MESH SET

	switch (a_create_info.pass_type)
	{
	case PASS_TYPE::GLOBAL:
		BB_LOG("GLOBAL pass unimplemented");
		desc_layouts[SPACE_PER_SCENE] = s_material_inst->scene_desc_layout;
		break;
	case PASS_TYPE::SCENE:
		desc_layouts[SPACE_PER_SCENE] = s_material_inst->scene_desc_layout;
		break;
	default:
		BB_ASSERT(false, "unknown material pass");
		break;
	}

	uint32_t layouts = 3;

	switch (a_create_info.material_type)
	{
	case MATERIAL_TYPE::MATERIAL_2D:
		BB_LOG("SCENE_2D material unimplemented");
		break;
	case MATERIAL_TYPE::MATERIAL_3D:
		desc_layouts[SPACE_PER_MATERIAL] = s_material_inst->material_desc_layout;
		++layouts;
		break;
	case MATERIAL_TYPE::NONE:

		break;
	default:
		BB_ASSERT(false, "unknown material type");
		break;
	}

	const ShaderEffectList shader_effects = CreateShaderEffects_impl(a_temp_arena, a_create_info.shader_infos, desc_layouts, layouts);

	return CreateMaterial_impl(shader_effects.vertex, shader_effects.fragment, shader_effects.geometry, a_create_info.pass_type, a_create_info.material_type, a_create_info.user_data_size, a_create_info.cpu_writeable, a_name);
}

MaterialHandle Material::CreateMaterialInstance(const MasterMaterialHandle a_master_material)
{
	const MasterMaterial& master = s_material_inst->material_map.find(a_master_material);
	MaterialInstance mat;
	mat.master_handle = a_master_material;
	mat.user_data_size = master.user_data_size;

	GPUBufferCreateInfo create_buffer;
	create_buffer.name = master.name.c_str();
	create_buffer.type = BUFFER_TYPE::UNIFORM;
	create_buffer.size = mat.user_data_size;
	create_buffer.host_writable = master.cpu_writeable;
	mat.buffer = CreateGPUBuffer(create_buffer);
	if (create_buffer.host_writable)
		mat.mapper_ptr = MapGPUBuffer(mat.buffer);
	else
		mat.mapper_ptr = nullptr;

	const MaterialHandle material_index = MaterialHandle(s_material_inst->material_instances.emplace(mat));

	DescriptorWriteBufferInfo write_buffer;
	write_buffer.descriptor_layout = s_material_inst->material_desc_layout;
	write_buffer.allocation = s_material_inst->material_desc_allocation;
	write_buffer.binding = PER_MATERIAL_BINDING;
	write_buffer.descriptor_index = static_cast<uint32_t>(material_index.handle);
	write_buffer.buffer_view.buffer = mat.buffer;
	write_buffer.buffer_view.size = mat.user_data_size;
	write_buffer.buffer_view.offset = 0;
	DescriptorWriteUniformBuffer(write_buffer);

	return material_index;
}

void Material::FreeMaterialInstance(const MaterialHandle a_material)
{
	MaterialInstance& mat = s_material_inst->material_instances.find(a_material.handle);
	if (mat.mapper_ptr)
		UnmapGPUBuffer(mat.buffer);
	FreeGPUBuffer(mat.buffer);
	mat = {};
}

void Material::WriteMaterial(const MaterialHandle a_material, const RCommandList a_list, const GPUBuffer a_src_buffer, const size_t a_src_offset)
{
	const MaterialInstance& mat = s_material_inst->material_instances.find(a_material.handle);
	BB_WARNING(!mat.mapper_ptr, "trying to write to a material that is meant to be CPU writeable", WarningType::OPTIMIZATION);
	RenderCopyBufferRegion copy_region;
	copy_region.size = mat.user_data_size;
	copy_region.src_offset = a_src_offset;
	copy_region.dst_offset = 0;
	RenderCopyBuffer copy_buffer;
	copy_buffer.src = a_src_buffer;
	copy_buffer.dst = mat.buffer;
	copy_buffer.regions = Slice(&copy_region, 1);
	CopyBuffer(a_list, copy_buffer);
}

void Material::WriteMaterialCPU(const MaterialHandle a_material, const void* a_memory, const size_t a_memory_size)
{
	const MaterialInstance& mat = s_material_inst->material_instances.find(a_material.handle);
	BB_ASSERT(mat.mapper_ptr, "trying to write to a material that is not CPU writeable");
	memcpy(mat.mapper_ptr, a_memory, a_memory_size);
}

RPipelineLayout Material::BindMaterial(const RCommandList a_list, const MasterMaterialHandle a_material)
{
    const MasterMaterial& inst = s_material_inst->material_map.find(a_material);
	return BindShaders(a_list, inst.shaders.vertex, inst.shaders.fragment_pixel, inst.shaders.geometry);
}

const DescriptorAllocation& Material::GetMaterialDescAllocation()
{
	return s_material_inst->material_desc_allocation;
}

MasterMaterialHandle Material::GetDefaultMasterMaterial(const PASS_TYPE a_pass_type, const MATERIAL_TYPE a_material_type)
{
	return GetDefaultMasterMaterial_impl(a_pass_type, a_material_type);
}

const MasterMaterial& Material::GetMasterMaterial(const MasterMaterialHandle a_master_material)
{
	return s_material_inst->material_map.find(a_master_material);
}

ConstSlice<CachedShaderInfo> Material::GetAllCachedShaders()
{
	return s_material_inst->shader_effects.const_slice();
}

ConstSlice<MasterMaterial> Material::GetAllMasterMaterials()
{
	return s_material_inst->material_map.const_slice();
}

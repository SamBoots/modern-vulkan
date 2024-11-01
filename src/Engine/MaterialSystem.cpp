#include "MaterialSystem.hpp"
#include "SceneHierarchy.hpp"
#include "Renderer.hpp"
#include "Program.h"
#include "Storage/Hashmap.h"

using namespace BB;

BB_STATIC_ASSERT(sizeof(ShaderIndices) == sizeof(ShaderIndices2D), "shaderindices not the same sizeof.");

constexpr size_t PUSH_CONSTANT_SPACE_SIZE = sizeof(ShaderIndices);
// store this to avoid confusion later when adding more.
constexpr uint32_t CURRENT_DESC_LAYOUT_COUNT = 3;

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
	StaticSlotmap<MaterialInstance, MaterialHandle> material_map;
	StaticArray<CachedShaderInfo> shader_effects;
	StaticOL_HashMap<uint64_t, ShaderEffectHandle> shader_effect_cache;

	RDescriptorLayout scene_desc_layout;

	MaterialHandle default_materials[static_cast<uint32_t>(PASS_TYPE::ENUM_SIZE)][static_cast<uint32_t>(MATERIAL_TYPE::ENUM_SIZE)];
};

static MaterialSystem_inst* s_material_inst;

static inline MaterialHandle& GetDefaultMaterial_impl(const PASS_TYPE a_pass_type, const MATERIAL_TYPE a_material_type)
{
	return s_material_inst->default_materials[static_cast<uint32_t>(a_pass_type)][static_cast<uint32_t>(a_material_type)];
}

static inline MaterialHandle CreateMaterial_impl(const Slice<ShaderEffectHandle> a_shaders, const PASS_TYPE a_pass_type, const MATERIAL_TYPE a_material_type, const StringView name = "default")
{
	const MaterialHandle material = s_material_inst->material_map.emplace();
	MaterialInstance& inst = s_material_inst->material_map.find(material);
	inst.name = name;
	inst.shader_effect_count = a_shaders.size();
	for (size_t i = 0; i < a_shaders.size(); i++)
	{
		inst.shader_effects[i] = a_shaders[i];
	}
	inst.pass_type = a_pass_type;
	inst.material_type = a_material_type;
	inst.handle = material;
	return material;
}

static Slice<ShaderEffectHandle> CreateShaderEffects_impl(MemoryArena& a_temp_arena, const Slice<MaterialShaderCreateInfo> a_shader_effects_info, const ShaderDescriptorLayouts& a_desc_layouts)
{
	ShaderEffectHandle* return_handles = ArenaAllocArr(a_temp_arena, ShaderEffectHandle, a_shader_effects_info.size());
	CreateShaderEffectInfo* shader_effects = ArenaAllocArr(a_temp_arena, CreateShaderEffectInfo, a_shader_effects_info.size());
	size_t created_shader_effect_count = 0;

	MemoryArenaMarker memory_pos_before_shader_read = MemoryArenaGetMemoryMarker(a_temp_arena);
	StringView previous_shader_path;
	Buffer shader_buffer;

	for (size_t i = 0; i < a_shader_effects_info.size(); i++)
	{
		const MaterialShaderCreateInfo& info = a_shader_effects_info[i];

		if (const ShaderEffectHandle* found_shader = s_material_inst->shader_effect_cache.find(ShaderEffectHash(info)))
		{
			return_handles[i] = *found_shader;
		}
		else
		{
			return_handles[i] = {};
			if (previous_shader_path != info.path)
			{
				MemoryArenaSetMemoryMarker(a_temp_arena, memory_pos_before_shader_read);
				shader_buffer = ReadOSFile(a_temp_arena, info.path.c_str());
				previous_shader_path = info.path;
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
			shader_info.desc_layout_count = CURRENT_DESC_LAYOUT_COUNT;
		}
	}

	// all shaders already exist, return them.
	if (created_shader_effect_count == 0)
	{
		return Slice(return_handles, a_shader_effects_info.size());
	}

	ShaderEffectHandle* created_handles = ArenaAllocArr(a_temp_arena, ShaderEffectHandle, created_shader_effect_count);
	bool success = CreateShaderEffect(a_temp_arena, Slice(shader_effects, created_shader_effect_count), created_handles, false);
	if (!success)
	{
		BB_WARNING(false, "Material created failed due to failing to create shader effects", WarningType::MEDIUM);
		return Slice<ShaderEffectHandle>();
	}
	else
	{
		// all created shaders are filled, send them.
		if (a_shader_effects_info.size() == created_shader_effect_count)
		{
			for (size_t i = 0; i < created_shader_effect_count; i++)
			{
				const CachedShaderInfo shader_info = { created_handles[i], shader_effects[i] };
				s_material_inst->shader_effects.emplace_back(shader_info);
				s_material_inst->shader_effect_cache.insert(ShaderEffectHash(a_shader_effects_info[i]), shader_info.handle);
			}
			return Slice(created_handles, a_shader_effects_info.size());
		}

		// some are created and some are found. Sort them inside the empty slots in found_handles
		int created_shader_i = 0;
		for (size_t i = 0; i < a_shader_effects_info.size(); i++)
		{
			if (return_handles[i].IsValid())
			{
				const CachedShaderInfo shader_info = { return_handles[i], shader_effects[i] };
				s_material_inst->shader_effects.emplace_back(shader_info);
			}
			else
			{
				const CachedShaderInfo shader_info = { created_handles[created_shader_i++], shader_effects[i] };
				s_material_inst->shader_effects.emplace_back(shader_info);
				s_material_inst->shader_effect_cache.insert(ShaderEffectHash(a_shader_effects_info[i]), shader_info.handle);
				return_handles[i] = shader_info.handle;
			}
		}

	}
	return Slice(return_handles, a_shader_effects_info.size());
}

void Material::InitMaterialSystem(MemoryArena& a_arena, const MaterialSystemCreateInfo& a_create_info)
{
	BB_ASSERT(a_create_info.default_2d_vertex.stage == SHADER_STAGE::VERTEX, "default_2d_vertex.stage is not SHADER_STAGE::VERTEX");
	BB_ASSERT(a_create_info.default_2d_fragment.stage == SHADER_STAGE::FRAGMENT_PIXEL, "default_2d_fragment.stage is not SHADER_STAGE::FRAGMENT_PIXEL");
	BB_ASSERT(a_create_info.default_3d_vertex.stage == SHADER_STAGE::VERTEX, "default_3d_vertex.stage is not SHADER_STAGE::VERTEX");
	BB_ASSERT(a_create_info.default_3d_fragment.stage == SHADER_STAGE::FRAGMENT_PIXEL, "default_3d_fragment.stage is not SHADER_STAGE::FRAGMENT_PIXEL");

	s_material_inst = ArenaAllocType(a_arena, MaterialSystem_inst);
	s_material_inst->material_map.Init(a_arena, a_create_info.max_materials);
	s_material_inst->shader_effects.Init(a_arena, a_create_info.max_shader_effects);
	s_material_inst->shader_effect_cache.Init(a_arena, a_create_info.max_shader_effects);
	
	s_material_inst->scene_desc_layout = SceneHierarchy::GetSceneDescriptorLayout();

	MemoryArenaScope(a_arena)
	{
		MaterialCreateInfo material_info;
		material_info.material_type = MATERIAL_TYPE::MATERIAL_2D;
		MaterialShaderCreateInfo default_2d_shaders[]{ a_create_info.default_2d_vertex, a_create_info.default_2d_fragment };
		material_info.shader_infos = Slice(default_2d_shaders, _countof(default_2d_shaders));

		material_info.pass_type = PASS_TYPE::GLOBAL;
		GetDefaultMaterial_impl(PASS_TYPE::GLOBAL, MATERIAL_TYPE::MATERIAL_2D) = CreateMaterial(a_arena, material_info, "default global 2d");
		material_info.pass_type = PASS_TYPE::SCENE;
		GetDefaultMaterial_impl(PASS_TYPE::SCENE, MATERIAL_TYPE::MATERIAL_2D) = CreateMaterial(a_arena, material_info, "default scene 2d");

		MaterialShaderCreateInfo default_3d_shaders[]{ a_create_info.default_3d_vertex, a_create_info.default_3d_fragment };
		material_info.material_type = MATERIAL_TYPE::MATERIAL_3D;
		material_info.shader_infos = Slice(default_3d_shaders, _countof(default_3d_shaders));

		material_info.pass_type = PASS_TYPE::GLOBAL;
		GetDefaultMaterial_impl(PASS_TYPE::GLOBAL, MATERIAL_TYPE::MATERIAL_3D) = CreateMaterial(a_arena, material_info, "default global 3d");
		material_info.pass_type = PASS_TYPE::SCENE;
		GetDefaultMaterial_impl(PASS_TYPE::SCENE, MATERIAL_TYPE::MATERIAL_3D) = CreateMaterial(a_arena, material_info, "default scene 3d");
	}
}

MaterialHandle Material::CreateMaterial(MemoryArena& a_temp_arena, const MaterialCreateInfo& a_create_info, const StringView a_name)
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

	switch (a_create_info.material_type)
	{
	case MATERIAL_TYPE::MATERIAL_2D:
		BB_LOG("SCENE_2D material unimplemented");
		break;
	case MATERIAL_TYPE::MATERIAL_3D:
		BB_LOG("SCENE_3D material unimplemented");
		break;
	case MATERIAL_TYPE::NONE:

		break;
	default:
		BB_ASSERT(false, "unknown material type");
		break;
	}

	const Slice<ShaderEffectHandle> shader_effects = CreateShaderEffects_impl(a_temp_arena, a_create_info.shader_infos, desc_layouts);

	return CreateMaterial_impl(shader_effects, a_create_info.pass_type, a_create_info.material_type, a_name);
}

MaterialHandle Material::GetDefaultMaterial(const PASS_TYPE a_pass_type, const MATERIAL_TYPE a_material_type)
{
	return GetDefaultMaterial_impl(a_pass_type, a_material_type);
}

const MaterialInstance& Material::GetMaterialInstance(const MaterialHandle a_material)
{
	return s_material_inst->material_map.find(a_material);
}

Slice<const ShaderEffectHandle> Material::GetMaterialShaders(const MaterialHandle a_material)
{
	BB_ASSERT(a_material.IsValid(), "invalid material send!");
	const MaterialInstance& mat = s_material_inst->material_map.find(a_material);
	return Slice(mat.shader_effects.data(), mat.shader_effect_count);
}

Slice<const CachedShaderInfo> Material::GetAllCachedShaders()
{
	return s_material_inst->shader_effects.slice();
}

Slice<const MaterialInstance> Material::GetAllMaterials()
{
	return s_material_inst->material_map.slice();
}

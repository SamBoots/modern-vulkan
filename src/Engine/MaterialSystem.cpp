#include "MaterialSystem.hpp"
#include "SceneHierarchy.hpp"
#include "Renderer.hpp"
#include "Program.h"
#include "Storage/Hashmap.h"

using namespace BB;

BB_STATIC_ASSERT(sizeof(ShaderIndices) == sizeof(ShaderIndices2D), "shaderindices not the same sizeof.");

constexpr size_t PUSH_CONSTANT_SPACE_SIZE = sizeof(ShaderIndices);
// store this to avoid confusion later when adding more.
constexpr size_t CURRENT_DESC_LAYOUT_COUNT = 3;

struct MaterialSystem_inst
{
	StaticSlotmap<MaterialInstance, MaterialHandle> material_map;
	StaticArray<CachedShaderInfo> shader_effects;

	RDescriptorLayout scene_desc_layout;

	MaterialHandle default_materials[static_cast<size_t>(PASS_TYPE::ENUM_SIZE)][static_cast<size_t>(MATERIAL_TYPE::ENUM_SIZE)];
};

static MaterialSystem_inst* s_material_inst;

static inline MaterialHandle& GetDefaultMaterial_impl(const PASS_TYPE a_pass_type, const MATERIAL_TYPE a_material_type)
{
	return s_material_inst->default_materials[static_cast<uint32_t>(a_pass_type)][static_cast<uint32_t>(a_material_type)];
}

static inline MaterialHandle CreateMaterial_impl(const ShaderEffectHandle a_vertex, const ShaderEffectHandle a_fragment, const PASS_TYPE a_pass_type, const MATERIAL_TYPE a_material_type, const StringView name = "default")
{
	const MaterialHandle material = s_material_inst->material_map.emplace();
	MaterialInstance& inst = s_material_inst->material_map.find(material);
	inst.name = name;
	inst.shader_effects[0] = a_vertex;
	inst.shader_effects[1] = a_fragment;
	inst.shader_effect_count = 2;
	inst.pass_type = a_pass_type;
	inst.material_type = a_material_type;
	inst.handle = material;
	return material;
}

static Slice<ShaderEffectHandle> CreateShaderEffects_impl(MemoryArena& a_temp_arena, const Slice<const MaterialShaderCreateInfo> a_shader_effects_info, const ShaderDescriptorLayouts& a_desc_layouts)
{
	ShaderEffectHandle* handles = ArenaAllocArr(a_temp_arena, ShaderEffectHandle, a_shader_effects_info.size());

	MemoryArenaScope(a_temp_arena)
	{
		CreateShaderEffectInfo* shader_effects = ArenaAllocArr(a_temp_arena, CreateShaderEffectInfo, a_shader_effects_info.size());
		size_t shader_effect_count = 0;


		MemoryArenaMarker memory_pos_before_shader_read = MemoryArenaGetMemoryMarker(a_temp_arena);
		StringView previous_shader_path;
		Buffer shader_buffer;

		for (size_t i = 0; i < a_shader_effects_info.size(); i++)
		{
			// TODO: do a search, if found shader is already compiled then continue.

			const MaterialShaderCreateInfo& info = a_shader_effects_info[i];

			if (previous_shader_path != info.path)
			{
				MemoryArenaSetMemoryMarker(a_temp_arena, memory_pos_before_shader_read);
				shader_buffer = ReadOSFile(a_temp_arena, info.path.c_str());
				previous_shader_path = info.path;
			}

			const StringView name = info.path.c_str() + info.path.find_last_of('/');

			CreateShaderEffectInfo& shader_info = shader_effects[shader_effect_count++];
			shader_info.name = name.c_str();
			shader_info.shader_entry = info.entry.c_str();
			shader_info.shader_data = shader_buffer;
			shader_info.stage = info.stage;
			shader_info.next_stages = info.next_stages;
			shader_info.push_constant_space = PUSH_CONSTANT_SPACE_SIZE;
			shader_info.desc_layouts = a_desc_layouts;
			shader_info.desc_layout_count = CURRENT_DESC_LAYOUT_COUNT;
		}

		bool success = CreateShaderEffect(a_temp_arena, Slice(shader_effects, shader_effect_count), handles, true);
		if (!success)
		{
			BB_WARNING(false, "Material created failed due to failing to create shader effects", WarningType::MEDIUM);
			return Slice<ShaderEffectHandle>();
		}
		else
		{
			for (size_t i = 0; i < shader_effect_count; i++)
			{
				CachedShaderInfo shader_info = { handles[i], shader_effects[i] };
				s_material_inst->shader_effects.emplace_back(shader_info);
			}
		}
	}

	return Slice(handles, a_shader_effects_info.size());
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
	
	s_material_inst->scene_desc_layout = SceneHierarchy::GetSceneDescriptorLayout();

	MemoryArenaScope(a_arena)
	{
		MaterialCreateInfo material_info;
		material_info.material_type = MATERIAL_TYPE::MATERIAL_2D;
		material_info.vertex_shader_info = a_create_info.default_2d_vertex;
		material_info.fragment_shader_info = a_create_info.default_2d_fragment;

		material_info.pass_type = PASS_TYPE::GLOBAL;
		GetDefaultMaterial_impl(PASS_TYPE::GLOBAL, MATERIAL_TYPE::MATERIAL_2D) = CreateMaterial(a_arena, material_info, "default global 2d");
		material_info.pass_type = PASS_TYPE::SCENE;
		GetDefaultMaterial_impl(PASS_TYPE::SCENE, MATERIAL_TYPE::MATERIAL_2D) = CreateMaterial(a_arena, material_info, "default global 2d");


		material_info.material_type = MATERIAL_TYPE::MATERIAL_3D;
		material_info.vertex_shader_info = a_create_info.default_3d_vertex;
		material_info.fragment_shader_info = a_create_info.default_3d_fragment;

		material_info.pass_type = PASS_TYPE::GLOBAL;
		GetDefaultMaterial_impl(PASS_TYPE::GLOBAL, MATERIAL_TYPE::MATERIAL_3D) = CreateMaterial(a_arena, material_info, "default global 3d");
		material_info.pass_type = PASS_TYPE::SCENE;
		GetDefaultMaterial_impl(PASS_TYPE::SCENE, MATERIAL_TYPE::MATERIAL_3D) = CreateMaterial(a_arena, material_info, "default global 3d");
	}
}

MaterialHandle Material::CreateMaterial(MemoryArena& a_temp_arena, const MaterialCreateInfo& a_create_info, const StringView a_name)
{
	BB_ASSERT(a_create_info.vertex_shader_info.stage == SHADER_STAGE::VERTEX, "vertex_shader_info.stage is not SHADER_STAGE::VERTEX");
	BB_ASSERT(a_create_info.fragment_shader_info.stage == SHADER_STAGE::FRAGMENT_PIXEL, "fragment_shader_info.stage is not SHADER_STAGE::FRAGMENT_PIXEL");

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

	const MaterialShaderCreateInfo shader_effects_info[2]{ a_create_info.vertex_shader_info, a_create_info.fragment_shader_info };
	const Slice<ShaderEffectHandle> shader_effects = CreateShaderEffects_impl(a_temp_arena, Slice(shader_effects_info, _countof(shader_effects_info)), desc_layouts);

	return CreateMaterial_impl(shader_effects[0], shader_effects[1], a_create_info.pass_type, a_create_info.material_type, a_name);
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
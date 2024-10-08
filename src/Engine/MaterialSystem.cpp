#include "MaterialSystem.hpp"
#include "SceneHierarchy.hpp"
#include "Renderer.hpp"
#include "Program.h"
#include "Storage/Hashmap.h"

#include <tuple>

using namespace BB;

BB_STATIC_ASSERT(sizeof(ShaderIndices) == sizeof(ShaderIndices2D), "shaderindices not the same sizeof.");

constexpr size_t PUSH_CONSTANT_SPACE_SIZE = sizeof(ShaderIndices);
// store this to avoid confusion later when adding more.
constexpr size_t CURRENT_DESC_LAYOUT_COUNT = 3;
constexpr size_t MAX_SHADER_EFFECTS_PER_MATERIAL = 4;

struct sMaterial
{
	StringView name;
	FixedArray<ShaderEffectHandle, MAX_SHADER_EFFECTS_PER_MATERIAL> shader_effects;
	size_t shader_effect_count;
	PASS_TYPE pass_type;
	MATERIAL_TYPE material_type;
};

struct MaterialSystem_inst
{
	StaticSlotmap<sMaterial, MaterialHandle> material_map;
	StaticArray<std::tuple<ShaderEffectHandle, MaterialShaderCreateInfo>> shader_effects;

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
	sMaterial material;
	material.name = name;
	material.shader_effects[0] = a_vertex;
	material.shader_effects[1] = a_fragment;
	material.shader_effect_count = 2;
	material.pass_type = a_pass_type;
	material.material_type = a_material_type;
	return s_material_inst->material_map.insert(material);
}

static Slice<ShaderEffectHandle> CreateShaderEffects_impl(MemoryArena& a_temp_arena, const Slice<const MaterialShaderCreateInfo> a_shader_effects_info)
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

			const StringView name = info.path.c_str() + info.path.find_last_of('\\');

			CreateShaderEffectInfo& shader_info = shader_effects[shader_effect_count++];
			shader_info.name = name.c_str();
			shader_info.shader_entry = info.entry.c_str();
			shader_info.shader_data = shader_buffer;
			shader_info.stage = SHADER_STAGE::VERTEX;
			shader_info.next_stages = static_cast<SHADER_STAGE_FLAGS>(SHADER_STAGE::FRAGMENT_PIXEL);
			shader_info.push_constant_space = PUSH_CONSTANT_SPACE_SIZE;
			shader_info.desc_layouts[0] = s_material_inst->scene_desc_layout;
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
				s_material_inst->shader_effects.emplace_back(handles[i]);
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

	const MaterialShaderCreateInfo shader_effects_info[]
	{ 
		a_create_info.default_2d_vertex, 
		a_create_info.default_2d_fragment,
		a_create_info.default_3d_vertex,
		a_create_info.default_3d_fragment,
	};
	MemoryArenaScope(a_arena)
	{
		constexpr size_t VERT_2D = 0;
		constexpr size_t FRAG_2D = 1;
		constexpr size_t VERT_3D = 2;
		constexpr size_t FRAG_3D = 3;

		const Slice<ShaderEffectHandle> effects = CreateShaderEffects_impl(a_arena, Slice(shader_effects_info, _countof(shader_effects_info)));

		GetDefaultMaterial_impl(PASS_TYPE::GLOBAL, MATERIAL_TYPE::MATERIAL_2D) = 
			CreateMaterial_impl(effects[VERT_2D], effects[FRAG_2D], PASS_TYPE::GLOBAL, MATERIAL_TYPE::MATERIAL_2D, "default global 2d");
		GetDefaultMaterial_impl(PASS_TYPE::SCENE, MATERIAL_TYPE::MATERIAL_2D) = 
			CreateMaterial_impl(effects[VERT_2D], effects[FRAG_2D], PASS_TYPE::SCENE, MATERIAL_TYPE::MATERIAL_2D, "default scene 2d");

		GetDefaultMaterial_impl(PASS_TYPE::GLOBAL, MATERIAL_TYPE::MATERIAL_3D) = 
			CreateMaterial_impl(effects[VERT_3D], effects[FRAG_3D], PASS_TYPE::GLOBAL, MATERIAL_TYPE::MATERIAL_3D, "default global 3d");
		GetDefaultMaterial_impl(PASS_TYPE::SCENE, MATERIAL_TYPE::MATERIAL_3D) = 
			CreateMaterial_impl(effects[VERT_3D], effects[FRAG_3D], PASS_TYPE::SCENE, MATERIAL_TYPE::MATERIAL_3D, "default scene 3d");
	}
}

MaterialHandle Material::CreateMaterial(MemoryArena& a_temp_arena, const MaterialCreateInfo& a_create_info, const StringView a_name)
{
	BB_ASSERT(a_create_info.vertex_shader_info.stage == SHADER_STAGE::VERTEX, "vertex_shader_info.stage is not SHADER_STAGE::VERTEX");
	BB_ASSERT(a_create_info.fragment_shader_info.stage == SHADER_STAGE::FRAGMENT_PIXEL, "fragment_shader_info.stage is not SHADER_STAGE::FRAGMENT_PIXEL");

	FixedArray<RDescriptorLayout, SHADER_DESC_LAYOUT_MAX> desc_layouts = {
			GetStaticSamplerDescriptorLayout(),
			GetGlobalDescriptorLayout(),
			RDescriptorLayout(BB_INVALID_HANDLE_64),	// PER SCENE SET
			RDescriptorLayout(BB_INVALID_HANDLE_64),	// PER MATERIAL SET
			RDescriptorLayout(BB_INVALID_HANDLE_64) };	// PER MESH SET

	switch (a_create_info.pass_type)
	{
	case PASS_TYPE::GLOBAL:
		BB_LOG("GLOBAL pass unimplemented");
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
	const Slice<ShaderEffectHandle> shader_effects = CreateShaderEffects_impl(a_temp_arena, Slice(shader_effects_info, _countof(shader_effects_info)));

	return CreateMaterial_impl(shader_effects[0], shader_effects[1], a_create_info.pass_type, a_create_info.material_type, a_name);
}

MaterialHandle Material::GetDefaultMaterial(const PASS_TYPE a_pass_type, const MATERIAL_TYPE a_material_type)
{
	return GetDefaultMaterial_impl(a_pass_type, a_material_type);
}

Slice<const ShaderEffectHandle> Material::GetMaterialShaders(const MaterialHandle a_material)
{
	BB_ASSERT(a_material.IsValid(), "invalid material send!");
	const sMaterial& mat = s_material_inst->material_map.find(a_material);
	return Slice(mat.shader_effects.data(), mat.shader_effect_count);
}

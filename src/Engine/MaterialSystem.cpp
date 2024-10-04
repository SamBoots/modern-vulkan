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

struct sMaterial
{
	ShaderEffectHandle vertex;
	ShaderEffectHandle fragment;
	PASS_TYPE pass_type;
	MATERIAL_TYPE material_type;
};

struct MaterialSystem_inst
{
	StaticSlotmap<sMaterial, MaterialHandle> material_map;
	StaticArray<std::tuple<ShaderEffectHandle, MaterialShaderCreateInfo>> shader_effects;

	RDescriptorLayout scene_desc_layout;
};

static MaterialSystem_inst* s_material_inst;

void Material::InitMaterialSystem(MemoryArena& a_arena, const MaterialSystemCreateInfo& a_create_info)
{
	s_material_inst = ArenaAllocType(a_arena, MaterialSystem_inst);
	s_material_inst->material_map.Init(a_arena, a_create_info.max_materials);
	s_material_inst->shader_effects.Init(a_arena, a_create_info.max_shader_effects);
	
	s_material_inst->scene_desc_layout = SceneHierarchy::GetSceneDescriptorLayout();
}


MaterialHandle Material::CreateMaterial(MemoryArena& a_temp_arena, const MaterialCreateInfo& a_create_info, const StringView a_name)
{
	FixedArray<RDescriptorLayout, SHADER_DESC_LAYOUT_MAX> desc_layouts = {
			GetStaticSamplerDescriptorLayout(),
			GetGlobalDescriptorLayout(),
			RDescriptorLayout(BB_INVALID_HANDLE_64),	// PER SCENE SET
			RDescriptorLayout(BB_INVALID_HANDLE_64),	// PER MATERIAL SET
			RDescriptorLayout(BB_INVALID_HANDLE_64) };	// PER MESH SET

	switch (a_create_info.pass_type)
	{
	case PASS_TYPE::GLOBAL:
		BB_UNIMPLEMENTED();
		break;
	case PASS_TYPE::SCENE:
		desc_layouts[SPACE_PER_SCENE] = s_material_inst->scene_desc_layout;
		break;
	default:
		break;
	}

	switch (a_create_info.material_type)
	{
	case MATERIAL_TYPE::SCENE_2D:
		BB_LOG("SCENE_2D material unimplemented");
		break;
	case MATERIAL_TYPE::SCENE_3D:
		BB_LOG("SCENE_3D material unimplemented");
		break;
	default:
		break;
	}

	CreateShaderEffectInfo shader_effects_info[2];
	size_t shader_effect_count = 0;

	MemoryArenaMarker memory_pos_before_shader_read = MemoryArenaGetMemoryMarker(a_temp_arena);
	StringView previous_shader_path;
	Buffer shader_buffer;

	{
		const MaterialShaderCreateInfo& info = a_create_info.vertex_shader_info;

		if (previous_shader_path != info.shader_path)
		{
			MemoryArenaSetMemoryMarker(a_temp_arena, memory_pos_before_shader_read);
			shader_buffer = ReadOSFile(a_temp_arena, info.shader_path.c_str());
			previous_shader_path = info.shader_path;
		}

		CreateShaderEffectInfo& vertex_info = shader_effects_info[shader_effect_count++];
		vertex_info.name = a_name.c_str();
		vertex_info.shader_entry = info.shader_path.c_str();
		vertex_info.shader_data = shader_buffer;
		vertex_info.stage = SHADER_STAGE::VERTEX;
		vertex_info.next_stages = static_cast<SHADER_STAGE_FLAGS>(SHADER_STAGE::FRAGMENT_PIXEL);
		vertex_info.push_constant_space = PUSH_CONSTANT_SPACE_SIZE;
		vertex_info.desc_layouts[0] = s_material_inst->scene_desc_layout;
		vertex_info.desc_layout_count = CURRENT_DESC_LAYOUT_COUNT;
	}


	{
		const MaterialShaderCreateInfo& info = a_create_info.fragment_shader_info;

		if (previous_shader_path != info.shader_path)
		{
			MemoryArenaSetMemoryMarker(a_temp_arena, memory_pos_before_shader_read);
			shader_buffer = ReadOSFile(a_temp_arena, info.shader_path.c_str());
			previous_shader_path = info.shader_path;
		}

		CreateShaderEffectInfo& fragment_info = shader_effects_info[shader_effect_count++];
		fragment_info.name = a_name.c_str();
		fragment_info.shader_entry = info.shader_path.c_str();
		fragment_info.shader_data = shader_buffer;
		fragment_info.stage = SHADER_STAGE::FRAGMENT_PIXEL;
		fragment_info.next_stages = static_cast<SHADER_STAGE_FLAGS>(SHADER_STAGE::NONE);
		fragment_info.push_constant_space = PUSH_CONSTANT_SPACE_SIZE;
		fragment_info.desc_layouts = desc_layouts;
		fragment_info.desc_layout_count = CURRENT_DESC_LAYOUT_COUNT;
	}

	ShaderEffectHandle* shader_effects = ArenaAllocArr(a_temp_arena, ShaderEffectHandle, shader_effect_count);
	bool success = CreateShaderEffect(a_temp_arena, Slice(shader_effects_info, shader_effect_count), shader_effects, true);
	if (!success)
	{
		BB_WARNING(false, "Material created failed due to failing to create shader effects", WarningType::MEDIUM);
		return MaterialHandle(BB_INVALID_HANDLE_64);
	}

	s_material_inst->shader_effects.emplace_back(shader_effects[0], a_create_info.vertex_shader_info);
	s_material_inst->shader_effects.emplace_back(shader_effects[1], a_create_info.fragment_shader_info);

	sMaterial material;
	material.vertex = shader_effects[0];
	material.fragment = shader_effects[1];
	material.pass_type = material.pass_type;
	material.material_type = a_create_info.material_type;
	return s_material_inst->material_map.insert(material);
}

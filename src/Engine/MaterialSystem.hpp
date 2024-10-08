#pragma once
#include "Storage/BBString.h"
#include "Rendererfwd.hpp"

namespace BB
{
	using MaterialHandle = FrameworkHandle<struct MaterialHandleTag>;

	struct MaterialShaderCreateInfo
	{
		StringView path;
		StringView entry;
		SHADER_STAGE stage;
		SHADER_STAGE_FLAGS next_stages;
	};

	enum class PASS_TYPE
	{
		GLOBAL,
		SCENE,
		ENUM_SIZE
	};

	enum class MATERIAL_TYPE
	{
		SCENE_3D,
		SCENE_2D,
		ENUM_SIZE
	};

	struct MaterialCreateInfo
	{
		MaterialShaderCreateInfo vertex_shader_info;
		MaterialShaderCreateInfo fragment_shader_info;
		PASS_TYPE pass_type;
		MATERIAL_TYPE material_type;
	};

	struct MaterialSystemCreateInfo
	{
		uint32_t max_materials;
		uint32_t max_shader_effects;

		MaterialShaderCreateInfo default_3d_vertex;
		MaterialShaderCreateInfo default_3d_fragment;

		MaterialShaderCreateInfo default_2d_vertex;
		MaterialShaderCreateInfo default_2d_fragment;
	};

	namespace Material
	{
		void InitMaterialSystem(MemoryArena& a_arena, const MaterialSystemCreateInfo& a_create_info);

		MaterialHandle CreateMaterial(MemoryArena& a_temp_arena, const MaterialCreateInfo& a_create_info, const StringView a_name);
		MaterialHandle GetDefaultMaterial(const PASS_TYPE a_pass_type, const MATERIAL_TYPE a_material_type);
	};

}

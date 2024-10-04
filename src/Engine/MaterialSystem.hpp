#pragma once
#include "Storage/BBString.h"
#include "Rendererfwd.hpp"

namespace BB
{
	using MaterialHandle = FrameworkHandle<struct MaterialHandleTag>;

	struct MaterialShaderCreateInfo
	{
		StringView shader_path;
		StringView shader_entry;
	};

	enum class PASS_TYPE
	{
		GLOBAL,
		SCENE
	};

	enum class MATERIAL_TYPE
	{
		SCENE_3D,
		SCENE_2D
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
	};

	namespace Material
	{
		void InitMaterialSystem(MemoryArena& a_arena, const MaterialSystemCreateInfo& a_create_info);

		MaterialHandle CreateMaterial(MemoryArena& a_temp_arena, const MaterialCreateInfo& a_create_info, const StringView a_name);
	};

}

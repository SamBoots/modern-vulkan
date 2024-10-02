#pragma once
#include "Storage/BBString.h"
#include "Rendererfwd.hpp"

namespace BB
{
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
		
	};

	namespace Material
	{
		void InitMaterialSystem();
	};

}

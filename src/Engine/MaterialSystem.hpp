#pragma once
#include "Storage/BBString.h"
#include "Storage/Pool.h"
#include "Rendererfwd.hpp"
#include "Enginefwd.hpp"

namespace BB
{
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
		MATERIAL_3D,
		MATERIAL_2D,
		NONE,
		ENUM_SIZE
	};

	struct MaterialCreateInfo
	{
		Slice<MaterialShaderCreateInfo> shader_infos;
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

	struct CachedShaderInfo
	{
		ShaderEffectHandle handle;
		CreateShaderEffectInfo create_info;
	};

	constexpr size_t MAX_SHADER_EFFECTS_PER_MATERIAL = 4;
	using MaterialShaderEffects = FixedArray<ShaderEffectHandle, MAX_SHADER_EFFECTS_PER_MATERIAL>;

	struct MaterialInstance
	{
		StringView name;
		MaterialShaderEffects shader_effects;
		size_t shader_effect_count;
		PASS_TYPE pass_type;
		MATERIAL_TYPE material_type;
		MaterialHandle handle;
	};

	namespace Material
	{
		void InitMaterialSystem(MemoryArena& a_arena, const MaterialSystemCreateInfo& a_create_info);

		MaterialHandle CreateMaterial(MemoryArena& a_temp_arena, const MaterialCreateInfo& a_create_info, const StringView a_name);
		MaterialHandle GetDefaultMaterial(const PASS_TYPE a_pass_type, const MATERIAL_TYPE a_material_type);
		const MaterialInstance& GetMaterialInstance(const MaterialHandle a_material);
 		Slice<const ShaderEffectHandle> GetMaterialShaders(const MaterialHandle a_material);
		Slice<const CachedShaderInfo> GetAllCachedShaders();
		Slice<const MaterialInstance> GetAllMaterials();
	};
}

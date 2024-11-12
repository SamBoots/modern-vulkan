#pragma once
#include "Storage/BBString.h"
#include "Storage/FreelistArray.hpp"
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
		uint32_t user_data_size;
		bool cpu_writeable;
	};

	struct MaterialSystemCreateInfo
	{
		uint32_t max_materials;
		uint32_t max_material_instances;
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

	struct MasterMaterial
	{
		StringView name;
		MaterialShaderEffects shader_effects;
		uint32_t shader_effect_count;
		PASS_TYPE pass_type;
		MATERIAL_TYPE material_type;
		
		uint32_t user_data_size;
		MasterMaterialHandle handle;
		bool cpu_writeable;
	};

	struct MaterialInstance
	{
		MasterMaterialHandle master_handle;
		uint32_t user_data_size;
		GPUBuffer buffer;
		void* mapper_ptr; // if true means the buffer is cpu writeable;
	};

	namespace Material
	{
		void InitMaterialSystem(MemoryArena& a_arena, const MaterialSystemCreateInfo& a_create_info);

		MasterMaterialHandle CreateMasterMaterial(MemoryArena& a_temp_arena, const MaterialCreateInfo& a_create_info, const StringView a_name);
		MaterialHandle CreateMaterialInstance(const MasterMaterialHandle a_master_material);
		void FreeMaterialInstance(const MaterialHandle a_material);
		void WriteMaterial(const MaterialHandle a_material, const RCommandList a_list, const GPUBuffer a_src_buffer, const size_t a_src_offset);
		void WriteMaterialCPU(const MaterialHandle a_material, const void* a_memory, const size_t a_memory_size);

		const DescriptorAllocation& GetMaterialDescAllocation();
		MasterMaterialHandle GetDefaultMasterMaterial(const PASS_TYPE a_pass_type, const MATERIAL_TYPE a_material_type);
		const MasterMaterial& GetMasterMaterial(const MasterMaterialHandle a_master_material);
 		Slice<const ShaderEffectHandle> GetMaterialShaders(const MasterMaterialHandle a_master_material);
		Slice<const CachedShaderInfo> GetAllCachedShaders();
		Slice<const MasterMaterial> GetAllMasterMaterials();
	};
}

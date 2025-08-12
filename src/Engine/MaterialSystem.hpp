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

	constexpr const uint32_t MAX_SHADER_ENTRY_SIZE = 64;

	struct CachedShaderInfo
	{
		ShaderEffectHandle handle;
		StackString<MAX_PATH_SIZE> path;
		StackString<MAX_SHADER_ENTRY_SIZE> entry;
		SHADER_STAGE stage;
		SHADER_STAGE_FLAGS next_stages;
	};

	struct MasterMaterial
	{
		StringView name;
		struct Shaders
		{
			ShaderEffectHandle vertex;
			ShaderEffectHandle fragment_pixel;
			ShaderEffectHandle geometry;
			CachedShaderInfo* vertex_info;
			CachedShaderInfo* fragment_pixel_info;
			CachedShaderInfo* geometry_info;
		} shaders;

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
		static inline const char* PASS_TYPE_STR(const PASS_TYPE a_pass)
		{
			switch (a_pass)
			{
                ENUM_CASE_STR(PASS_TYPE, GLOBAL);
                ENUM_CASE_STR(PASS_TYPE, SCENE);
                ENUM_CASE_STR_NOT_FOUND();
			}
		}

		static inline const char* MATERIAL_TYPE_STR(const MATERIAL_TYPE a_material)
		{
			switch (a_material)
			{
                ENUM_CASE_STR(MATERIAL_TYPE, MATERIAL_3D);
                ENUM_CASE_STR(MATERIAL_TYPE, MATERIAL_2D);
                ENUM_CASE_STR(MATERIAL_TYPE, NONE);
                ENUM_CASE_STR_NOT_FOUND();
			}
		}

		void InitMaterialSystem(MemoryArena& a_arena, const MaterialSystemCreateInfo& a_create_info);

		MasterMaterialHandle CreateMasterMaterial(MemoryArena& a_temp_arena, const MaterialCreateInfo& a_create_info, const StringView a_name);
		MaterialHandle CreateMaterialInstance(const MasterMaterialHandle a_master_material);
		void FreeMaterialInstance(const MaterialHandle a_material);
		void WriteMaterial(const MaterialHandle a_material, const RCommandList a_list, const GPUBuffer a_src_buffer, const size_t a_src_offset);
		void WriteMaterialCPU(const MaterialHandle a_material, const void* a_memory, const size_t a_memory_size);

		void BindMaterial(const RCommandList a_list, const MasterMaterialHandle a_material);

		MasterMaterialHandle GetDefaultMasterMaterial(const PASS_TYPE a_pass_type, const MATERIAL_TYPE a_material_type);
		const MasterMaterial& GetMasterMaterial(const MasterMaterialHandle a_master_material);
		ConstSlice<CachedShaderInfo> GetAllCachedShaders();
		ConstSlice<MasterMaterial> GetAllMasterMaterials();
	};
}

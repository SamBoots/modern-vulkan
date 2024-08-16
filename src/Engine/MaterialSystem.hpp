#pragma once
#include "Common.h"
#include "Storage/Slotmap.h"

namespace BB
{

	struct Material
	{
		const char* name;
		ShaderEffectHandle vertex_effect;
		ShaderEffectHandle fragment_effect;
	};

	struct CreateMaterialInfo
	{
		const char* name;
		ShaderEffectHandle vertex_effect;
		ShaderEffectHandle fragment_effect;
	};

	void InitializeMaterials(MemoryArena& a_memory_arena, const uint32_t a_max_materials, const CreateMaterialInfo& a_default_material);

	MaterialHandle CreateMaterial(const CreateMaterialInfo& a_info);
	void FreeMaterial(const MaterialHandle a_mat);
	Material& GetMaterial(const MaterialHandle a_mat);

	MaterialHandle GetDefaultMaterial();
}

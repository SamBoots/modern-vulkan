#pragma once
#include "Common.h"

namespace BB
{
	using MaterialHandle = FrameworkHandle<struct MaterialHandleTag>;

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

	class MaterialSystem
	{
	public:
		void Init(MemoryArena& a_memory_arena, const uint32_t a_max_materials);

		MaterialHandle CreateMaterial(const CreateMaterialInfo& a_info);
		Material& GetMaterial(const MaterialHandle a_mat);

	private:
		StaticSlotmap<Material, MaterialHandle> m_materials;
	};
}

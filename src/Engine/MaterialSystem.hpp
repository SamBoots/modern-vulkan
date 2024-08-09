#pragma once
#include "Common.h"
#include "Storage/Slotmap.h"

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
		void Init(MemoryArena& a_memory_arena, const uint32_t a_max_materials, const CreateMaterialInfo& a_default_material);

		MaterialHandle CreateMaterial(const CreateMaterialInfo& a_info);
		void FreeMaterial(const MaterialHandle a_mat);
		Material& GetMaterial(const MaterialHandle a_mat) const;

		MaterialHandle GetDefaultMaterial() const;

	private:
		StaticSlotmap<Material, MaterialHandle> m_materials;
		MaterialHandle m_default_material;
	};
}

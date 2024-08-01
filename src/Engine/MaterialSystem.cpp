#include "MaterialSystem.hpp"
#include "Storage/Slotmap.h"
#include "Rendererfwd.hpp"

using namespace BB;

void MaterialSystem::Init(MemoryArena& a_memory_arena, const uint32_t a_max_materials)
{
	m_materials.Init(a_memory_arena, a_max_materials);
}

MaterialHandle MaterialSystem::CreateMaterial(const CreateMaterialInfo& a_info)
{
	Material new_material;
	new_material.name = a_info.name;
	new_material.vertex_effect = new_material.vertex_effect;
	new_material.fragment_effect = new_material.fragment_effect;
	// TODO, test if the shaders are compatible

	return m_materials.insert(new_material);
}

Material& MaterialSystem::GetMaterial(const MaterialHandle a_mat)
{
	return m_materials.find(a_mat);
}

#include "MaterialSystem.hpp"
#include "Rendererfwd.hpp"

using namespace BB;

void MaterialSystem::Init(MemoryArena& a_memory_arena, const uint32_t a_max_materials, const CreateMaterialInfo& a_default_material)
{
	m_materials.Init(a_memory_arena, a_max_materials);
	m_default_material = CreateMaterial(a_default_material);
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

Material& MaterialSystem::GetMaterial(const MaterialHandle a_mat) const
{
	return m_materials.find(a_mat);
}

void MaterialSystem::FreeMaterial(const MaterialHandle a_mat)
{
	BB_ASSERT(a_mat != m_default_material, "trying to delete the default material!");
	m_materials.erase(a_mat);
}

MaterialHandle MaterialSystem::GetDefaultMaterial() const
{
	return m_default_material;
}

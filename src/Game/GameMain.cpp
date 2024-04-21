#include "GameMain.hpp"

using namespace BB;

bool DungeonGame::InitGame(MemoryArena& a_arena, const uint32_t a_scene_hierarchies)
{
	m_scene_hierarchies.Init(a_arena, a_scene_hierarchies);
	
	return true;
}

bool DungeonGame::Update(MemoryArena& a_temp_arena)
{
	return true;
}

void DungeonGame::Destroy()
{

}

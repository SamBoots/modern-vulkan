#pragma once
#include "Storage/Slotmap.h"
#include "Renderer.hpp"
#include "Storage/BBString.h"
#include "AssetLoader.hpp"
#include "GPUBuffers.hpp"

#include "ecs/EntityMap.hpp"
#include "ecs/EntityComponentSystem.hpp"
#include "ecs/systems/RenderSystem.hpp"

namespace BB
{
	class JsonParser;

	struct LightCreateInfo
	{
		LIGHT_TYPE light_type;      // 4
		float3 color;               // 16
		float3 pos;                 // 28
		float specular_strength;    // 32

		float radius_constant;      // 36
		float radius_linear;        // 40
		float radius_quadratic;     // 44

		float3 direction;           // 56
		float cutoff_radius;        // 60
	};

	struct SceneMeshCreateInfo
	{
		Mesh mesh;
		uint32_t index_start;
		uint32_t index_count;
		MasterMaterialHandle master_material;
		MeshMetallic material_data;
	};

	constexpr uint32_t STANDARD_ECS_OBJ_COUNT = 4096;

	struct SceneFrame
	{
		RenderSystemFrame render_frame;
	};

	class SceneHierarchy
	{
	public:
		friend class Editor;
		void Init(MemoryArena& a_arena, const uint32_t a_ecs_obj_max, const uint2 a_window_size, const uint32_t a_back_buffers, const StackString<32> a_name);
		static StaticArray<Asset::AsyncAsset> PreloadAssetsFromJson(MemoryArena& a_arena, const JsonParser& a_parsed_file);

		SceneFrame UpdateScene(const RCommandList a_list, class Viewport& a_viewport);
		bool DrawImgui(const RDescriptorIndex a_render_target, class Viewport& a_viewport);

		ECSEntity CreateEntity(const float3 a_position, const NameComponent& a_name, const ECSEntity a_parent = INVALID_ECS_OBJ);
		ECSEntity CreateEntityMesh(const float3 a_position, const SceneMeshCreateInfo& a_mesh_info, const char* a_name, const ECSEntity a_parent = INVALID_ECS_OBJ);
		ECSEntity CreateEntityViaModel(const Model& a_model, const float3 a_position, const char* a_name, const ECSEntity a_parent = INVALID_ECS_OBJ);
		ECSEntity CreateEntityAsLight(const LightCreateInfo& a_light_create_info, const char* a_name, const ECSEntity a_parent = INVALID_ECS_OBJ);


		static float4x4 CalculateLightProjectionView(const float3 a_pos, const float a_near, const float a_far);

		EntityComponentSystem& GetECS() { return m_ecs; }
	private:
		ECSEntity CreateEntityViaModelNode(const Model::Node& a_node, const ECSEntity a_parent);

		bool CreateLight(const ECSEntity a_entity, const LightCreateInfo& a_light_info);

		EntityComponentSystem m_ecs;
	};
}

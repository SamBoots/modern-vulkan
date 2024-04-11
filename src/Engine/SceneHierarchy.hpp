#pragma once
#include "Common.h"
#include "Storage/Slotmap.h"
#include "Transform.hpp"
#include "Renderer.hpp"
#include "AssetLoader.hpp"
#include "Storage/FixedArray.h"

namespace BB
{
	using SceneObjectHandle = FrameworkHandle<struct SceneObjectHandleTag>;
	
	constexpr uint32_t DEFAULT_SCENE_OBJ_MAX = 512;
	constexpr uint32_t SCENE_OBJ_CHILD_MAX = 256;

	struct SceneObject
	{
		const char* name;			// 8
		MeshHandle mesh_handle;		// 16
		uint32_t start_index;		// 20
		uint32_t index_count;		// 24

		RTexture albedo_texture;	// 28
		RTexture normal_texture;	// 32

		LightHandle light_handle;	// 40

		TransformHandle transform;	// 48

		SceneObjectHandle parent;	// 56
		uint32_t child_count;		// 60
		SceneObjectHandle childeren[SCENE_OBJ_CHILD_MAX];
	};
	
	class SceneHierarchy
	{
	public:
		void Init(MemoryArena& a_memory_arena, const StringView a_name, const uint32_t a_scene_obj_max = DEFAULT_SCENE_OBJ_MAX);
		void InitViaJson(MemoryArena& a_memory_arena, const FixedArray<ShaderEffectHandle, 2>& a_TEMP_shader_effects, const char* a_json_path, const uint32_t a_scene_obj_max = DEFAULT_SCENE_OBJ_MAX);

		void DrawSceneHierarchy(const RCommandList a_list, const RenderTarget a_render_target, const uint2 a_draw_area_size, const int2 a_draw_area_offset) const;
		void CreateSceneObjectViaModel(const Model& a_model, const FixedArray<ShaderEffectHandle, 2>& a_TEMP_shader_effects, const float3 a_position, const char* a_name);
		void CreateSceneObjectAsLight(const CreateLightInfo& a_light_create_info, const char* a_name);

		void SetView(const float4x4& a_view);
		void SetProjection(const float4x4& a_projection);

		void ImguiDisplaySceneHierarchy();

		RenderScene3DHandle GetRenderSceneHandle() const { return m_render_scene; }
		void SetClearColor(const float3 a_clear_color) { m_clear_color = a_clear_color; }

	private:
		SceneObjectHandle CreateSceneObjectEmpty(const SceneObjectHandle a_parent);
		SceneObjectHandle CreateSceneObjectViaModelNode(const Model& a_model, const FixedArray<ShaderEffectHandle, 2>& a_TEMP_shader_effects, const Model::Node& a_node, const SceneObjectHandle a_parent);
		void DrawSceneObject(const SceneObjectHandle a_scene_object, const float4x4& a_transform) const;
		void ImGuiDisplaySceneObject(const SceneObjectHandle a_object);

		//TODO, maybe remember all the transforms from the previous frames?
		TransformPool m_transform_pool;
		StaticSlotmap<SceneObject, SceneObjectHandle> m_scene_objects;

		uint32_t m_top_level_object_count;
		SceneObjectHandle* m_top_level_objects;

		RenderScene3DHandle m_render_scene;
		float3 m_clear_color;

		StringView m_scene_name;
	};
}

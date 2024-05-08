#pragma once
#include "Common.h"
#include "Storage/Slotmap.h"
#include "Transform.hpp"
#include "Renderer.hpp"
#include "Storage/Array.h"
#include "Storage/BBString.h"
#include "AssetLoader.hpp"

namespace BB
{
	using SceneObjectHandle = FrameworkHandle<struct SceneObjectHandleTag>;
	constexpr SceneObjectHandle INVALID_SCENE_OBJ = SceneObjectHandle(BB_INVALID_HANDLE_64);
	class JsonParser;

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
		MaterialHandle material;	// 40

		LightHandle light_handle;	// 48

		TransformHandle transform;	// 56

		SceneObjectHandle parent;	// 64
		void AddChild(const SceneObjectHandle a_child)
		{
			BB_ASSERT(child_count < SCENE_OBJ_CHILD_MAX, "Too many childeren for a single scene object!");
			childeren[child_count++] = a_child;
		}
		size_t child_count;			// 72
		SceneObjectHandle childeren[SCENE_OBJ_CHILD_MAX];
	};
	
	class SceneHierarchy
	{
	public:
		friend class Editor;
		void Init(MemoryArena& a_memory_arena, const StringView a_name, const uint32_t a_scene_obj_max = DEFAULT_SCENE_OBJ_MAX);
		void InitViaJson(MemoryArena& a_memory_arena, const char* a_json_path, const uint32_t a_scene_obj_max = DEFAULT_SCENE_OBJ_MAX);
		void InitViaJson(MemoryArena& a_memory_arena, const JsonParser& a_parsed_file, const uint32_t a_scene_obj_max = DEFAULT_SCENE_OBJ_MAX);
		static StaticArray<Asset::AsyncAsset> PreloadAssetsFromJson(MemoryArena& a_arena, const JsonParser& a_parsed_file);

		void DrawSceneHierarchy(const RCommandList a_list, const RenderTarget a_render_target, const uint2 a_draw_area_size, const int2 a_draw_area_offset);
		SceneObjectHandle CreateSceneObject(const float3 a_position, const char* a_name, const SceneObjectHandle a_parent = INVALID_SCENE_OBJ);
		SceneObjectHandle CreateSceneObjectMesh(const float3 a_position, const MeshHandle a_mesh, const uint32_t a_start_index, const uint32_t a_index_count, const MaterialHandle a_material, const char* a_name, const SceneObjectHandle a_parent = SceneObjectHandle(BB_INVALID_HANDLE_64));
		SceneObjectHandle CreateSceneObjectViaModel(const Model& a_model, const float3 a_position, const char* a_name, const SceneObjectHandle a_parent = INVALID_SCENE_OBJ);
		SceneObjectHandle CreateSceneObjectAsLight(const CreateLightInfo& a_light_create_info, const char* a_name, const SceneObjectHandle a_parent = INVALID_SCENE_OBJ);

		void SetSkyboxVertexShader(const ShaderEffectHandle a_vertex_shader)
		{
			m_skybox_shaders[0] = a_vertex_shader;
		}
		void SetSkyboxFragmentShader(const ShaderEffectHandle a_fragment_shader)
		{
			m_skybox_shaders[1] = a_fragment_shader;
		}

		void SetView(const float4x4& a_view);
		void SetProjection(const float4x4& a_projection);

		void SetClearColor(const float3 a_clear_color) { m_clear_color = a_clear_color; }

	private:
		void AddToDrawList(const SceneObject& scene_object, const float4x4& a_transform);
		SceneObjectHandle CreateSceneObjectViaModelNode(const Model& a_model, const Model::Node& a_node, const SceneObjectHandle a_parent);
		void DrawSceneObject(const SceneObjectHandle a_scene_object, const float4x4& a_transform);

		struct MeshDrawCall
		{
			MeshHandle mesh;
			MaterialHandle material;
			uint32_t index_start;
			uint32_t index_count;
			RTexture base_texture;
			RTexture normal_texture;
		};

		struct DrawList
		{
			MeshDrawCall* mesh_draw_call;
			ShaderTransform* transform;
			size_t size;
			size_t max_size;
		};
		DrawList m_draw_list;

		struct RenderFrameData
		{
			GPUBuffer per_frame_buffer;
			size_t per_frame_buffer_size;

			struct PerFrameBufferPart
			{
				uint32_t offset;
				uint32_t size;
			};
			PerFrameBufferPart scene_buffer;
			PerFrameBufferPart transform_buffer;
			PerFrameBufferPart light_buffer;
			DescriptorAllocation desc_alloc;
			uint64_t fence_value;
		};

		StringView m_scene_name;

		Scene3DInfo m_gpu_scene_info;

		//TODO, maybe remember all the transforms from the previous frames?
		TransformPool m_transform_pool;
		StaticSlotmap<SceneObject, SceneObjectHandle> m_scene_objects;

		StaticArray<RenderFrameData> m_render_frames;
		StaticSlotmap<Light, LightHandle> m_light_container;
		DrawList m_draw_list;

		uint2 m_previous_draw_area;
		RImage depth_image;
		RImageView depth_image_view;

		uint32_t m_top_level_object_count;
		SceneObjectHandle* m_top_level_objects;

		float3 m_clear_color;
		RTexture m_skybox;
		ShaderEffectHandle m_skybox_shaders[2]{};
	};
}

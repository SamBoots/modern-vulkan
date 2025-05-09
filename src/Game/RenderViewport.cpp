#include "RenderViewport.hpp"
#include "BBjson.hpp"
#include "BBThreadScheduler.hpp"
#include "Program.h"
#include "HID.h"
#include "AssetLoader.hpp"

#include "Math/Math.inl"
#include "Math/Collision.inl"

using namespace BB;

struct LoadAssetsAsync_params
{
	MemoryArena arena;
	Asset::AsyncAsset* assets;
	size_t asset_count;
};

static void CreateSceneHierarchyViaJson(MemoryArena& a_arena, SceneHierarchy& a_hierarchy, const uint2 a_window_size, const uint32_t a_back_buffer_count, const JsonParser& a_parsed_file)
{
	const JsonObject& scene_obj = a_parsed_file.GetRootNode()->GetObject().Find("scene")->GetObject();
	{
		a_hierarchy.Init(a_arena, STANDARD_ECS_OBJ_COUNT, a_window_size, a_back_buffer_count, scene_obj.Find("name")->GetString());
	}

	const JsonList& scene_objects = scene_obj.Find("scene_objects")->GetList();

	for (size_t i = 0; i < scene_objects.node_count; i++)
	{
		const JsonObject& sce_obj = scene_objects.nodes[i]->GetObject();
		const char* model_name = scene_objects.nodes[i]->GetObject().Find("file_name")->GetString();
		const char* obj_name = scene_objects.nodes[i]->GetObject().Find("file_name")->GetString();
		const Model* model = Asset::FindModelByName(model_name);
		BB_ASSERT(model != nullptr, "model failed to be found");
		const JsonList& position_list = sce_obj.Find("position")->GetList();
		BB_ASSERT(position_list.node_count == 3, "scene_object position in scene json is not 3 elements");
		float3 position;
		position.x = position_list.nodes[0]->GetNumber();
		position.y = position_list.nodes[1]->GetNumber();
		position.z = position_list.nodes[2]->GetNumber();
		a_hierarchy.CreateEntityViaModel(*model, position, obj_name);
	}

	const JsonList& lights = scene_obj.Find("lights")->GetList();
	for (size_t i = 0; i < lights.node_count; i++)
	{
		const JsonObject& light_obj = lights.nodes[i]->GetObject();
		LightCreateInfo light_info;

		const char* light_type = light_obj.Find("light_type")->GetString();
		if (strcmp(light_type, "spotlight") == 0)
			light_info.light_type = LIGHT_TYPE::SPOT_LIGHT;
		else if (strcmp(light_type, "pointlight") == 0)
			light_info.light_type = LIGHT_TYPE::POINT_LIGHT;
		else if (strcmp(light_type, "directional") == 0)
			light_info.light_type = LIGHT_TYPE::DIRECTIONAL_LIGHT;
		else
			BB_ASSERT(false, "invalid light type in json");

		const JsonList& position = light_obj.Find("position")->GetList();
		BB_ASSERT(position.node_count == 3, "light position in scene json is not 3 elements");
		light_info.pos.x = position.nodes[0]->GetNumber();
		light_info.pos.y = position.nodes[1]->GetNumber();
		light_info.pos.z = position.nodes[2]->GetNumber();

		const JsonList& color = light_obj.Find("color")->GetList();
		BB_ASSERT(color.node_count == 3, "light color in scene json is not 3 elements");
		light_info.color.x = color.nodes[0]->GetNumber();
		light_info.color.y = color.nodes[1]->GetNumber();
		light_info.color.z = color.nodes[2]->GetNumber();

		light_info.specular_strength = light_obj.Find("specular_strength")->GetNumber();
		light_info.radius_constant = light_obj.Find("constant")->GetNumber();
		light_info.radius_linear = light_obj.Find("linear")->GetNumber();
		light_info.radius_quadratic = light_obj.Find("quadratic")->GetNumber();

		if (light_info.light_type == LIGHT_TYPE::SPOT_LIGHT || light_info.light_type == LIGHT_TYPE::DIRECTIONAL_LIGHT)
		{
			const JsonList& spot_dir = light_obj.Find("direction")->GetList();
			BB_ASSERT(color.node_count == 3, "light direction in scene json is not 3 elements");
			light_info.direction.x = spot_dir.nodes[0]->GetNumber();
			light_info.direction.y = spot_dir.nodes[1]->GetNumber();
			light_info.direction.z = spot_dir.nodes[2]->GetNumber();

			light_info.cutoff_radius = light_obj.Find("cutoff_radius")->GetNumber();
		}

		const StringView light_name = light_obj.Find("name")->GetString();
		a_hierarchy.CreateEntityAsLight(light_info, light_name.c_str());
	}
}

bool RenderViewport::Init(const uint2 a_game_viewport_size, const uint32_t a_back_buffer_count, const StringView a_json_path)
{
	m_memory = MemoryArenaCreate();

	JsonParser json_file(a_json_path.c_str());
	json_file.Parse();
	MemoryArenaScope(m_memory)
	{
		auto viewer_list = SceneHierarchy::PreloadAssetsFromJson(m_memory, json_file);

		Asset::LoadAssets(m_memory, viewer_list.slice());
	}

	CreateSceneHierarchyViaJson(m_memory, m_scene_hierarchy, a_game_viewport_size, a_back_buffer_count, json_file);

	m_viewport.Init(a_game_viewport_size, int2(0, 0), "render viewport");
	m_camera.SetPosition(float3(0.f, 1.f, -1.f));
	m_camera.SetUp(float3(0.f, 1.f, 0.f));
	m_camera.SetSpeed(m_speed);
	return true;
}

bool RenderViewport::Update(const float a_delta_time)
{
	m_camera.Update(a_delta_time);
	if (!m_freeze_cam)
	{
		m_scene_hierarchy.GetECS().GetRenderSystem().SetView(m_camera.CalculateView(), m_camera.GetPosition());
	}
    if (m_selected_entity.IsValid())
    {
        m_scene_hierarchy.GetECS().DrawAABB(m_selected_entity, Color(255, 0, 255, 255));
    }
	DisplayImGuiInfo();
	return true;
}

bool RenderViewport::HandleInput(const float a_delta_time, const Slice<InputEvent> a_input_events)
{
	for (size_t i = 0; i < a_input_events.size(); i++)
	{
		const InputEvent& ip = a_input_events[i];
		if (ip.input_type == INPUT_TYPE::KEYBOARD)
		{
			const KeyInfo& ki = ip.key_info;
			float3 player_move{};
			if (ki.key_pressed)
			{
				switch (ki.scan_code)
				{
				case KEYBOARD_KEY::W:
					player_move.y = 1;
					break;
				case KEYBOARD_KEY::S:
					player_move.y = -1;
					break;
				case KEYBOARD_KEY::A:
					player_move.x = 1;
					break;
				case KEYBOARD_KEY::D:
					player_move.x = -1;
					break;
				case KEYBOARD_KEY::X:
					player_move.z = 1;
					break;
				case KEYBOARD_KEY::Z:
					player_move.z = -1;
					break;
				case KEYBOARD_KEY::F:
					m_freeze_cam = !m_freeze_cam;
					m_camera.SetVelocity();
					break;
				default:
					break;
				}
				player_move = player_move * a_delta_time;
			}
			if (!m_freeze_cam)
			{
				m_camera.Move(player_move);
			}
		}
		else if (ip.input_type == INPUT_TYPE::MOUSE)
		{
			const MouseInfo& mi = ip.mouse_info;
			const float2 mouse_move = (mi.move_offset * a_delta_time);

			if (mi.wheel_move)
			{
				m_speed = Clampf(
					m_speed + static_cast<float>(mi.wheel_move) * (m_speed * 0.02f),
					m_min_speed,
					m_max_speed);
				m_camera.SetSpeed(m_speed);
			}

			if (!m_freeze_cam)
			{
				m_camera.Rotate(mouse_move.x, mouse_move.y);
			}

            if (mi.left_pressed)
            {
                float2 mouse_pos_window;
                if (m_viewport.ScreenToViewportMousePosition(mi.mouse_pos, mouse_pos_window))
                {
                    const float4x4 view = m_scene_hierarchy.GetECS().GetRenderSystem().GetView();
                    const float3 dir = ScreenToWorldRaycast(mouse_pos_window, m_viewport.GetExtent(), m_scene_hierarchy.GetECS().GetRenderSystem().GetProjection(), view);
                    m_selected_entity = m_scene_hierarchy.GetECS().SelectEntityByRay(m_camera.GetPosition(), dir);
                }
            }
		}
	}

	return true;
}

#include "imgui.h"

void RenderViewport::DisplayImGuiInfo()
{
	if (ImGui::Begin("render viewport info"))
	{
		ImGui::Text("Freeze freecam: %s", m_freeze_cam ? "true" : "false");
		if (ImGui::Button("Toggle freecam freeze"))
		{
			m_freeze_cam = !m_freeze_cam;
		}

		if (ImGui::SliderFloat("Freecam speed", &m_speed, m_min_speed, m_max_speed))
		{
			m_camera.SetSpeed(m_max_speed);
		}
	}
	ImGui::End();
}

void RenderViewport::Destroy()
{

}

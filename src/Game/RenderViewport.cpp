#include "RenderViewport.hpp"
#include "BBjson.hpp"
#include "Program.h"
#include "HID.h"
#include "AssetLoader.hpp"

#include "Math/Math.inl"
#include "Math/Collision.inl"

#include "InputSystem.hpp"

#include "lua.h"
#include "lauxlib.h"
#include "lua/LuaTypes.hpp"

using namespace BB;

constexpr const char* lua_scene = R"( 
    local FreeCamera = {}
    FreeCamera.__index = FreeCamera

    function FreeCamera.new(a_pos, a_forward, a_right, a_up, a_speed)
    local camera = 
        {
            m_pos = a_pos or float3(0),
            m_forward = a_forward or float3(0, 0, -1),
            m_right = a_right or float3(1, 0, 0),
            m_up = a_up or float3(0, 1, 0),
            m_speed = a_speed or 1.0,
            m_min_speed = 1.0,
            m_max_speed = 100.0,
            m_velocity_speed = 25.0,
            m_yaw = 90,
            m_pitch = 0,
            m_velocity = float3(0)
        }
        setmetatable(camera, FreeCamera)
        return camera;
    end

    function FreeCamera:Move(a_x, a_y, a_z)
        local velocity = float3(0)
        velocity = velocity + self.m_forward * a_z
        velocity = velocity + float3Normalize(float3Cross(self.m_forward, self.m_up)) * a_x
        velocity = velocity + self.m_up * a_y
        self.m_velocity = self.m_velocity + velocity * self.m_speed;
    end

    function FreeCamera:Rotate(a_yaw, a_pitch)
        self.m_yaw = self.m_yaw + a_yaw
        self.m_pitch = self.m_pitch + a_pitch
        if self.m_pitch > 90.0 then
            self.m_pitch = 90.0
        elseif self.m_pitch < -90.0 then
            self.m_pitch = -90.0
        end
        
        local dir_x = math.cos(self.m_yaw) * math.cos(self.m_pitch)
        local dir_y = math.sin(self.m_pitch)
        local dir_z = math.sin(self.m_yaw) * math.cos(self.m_pitch)
        local direction = float3(dir_x, dir_y, dir_z)
        
        self.m_forward = float3Normalize(direction)
        local cross = float3Cross(self.m_up, self.m_forward)
        self.m_right = float3Normalize(cross)
    end
    
    function FreeCamera:Update(a_delta_time)
        local velocity = self.m_velocity * self.m_velocity_speed * a_delta_time;
        self.m_velocity = self.m_velocity - velocity;
        self.m_pos = self.m_pos + velocity;
    end

    function FreeCamera:AddSpeed(a_speed)
        local speed = self.m_speed + a_speed * ((self.m_speed + 2.2) * 0.022)
        if speed > self.m_max_speed then
            speed = self.m_max_speed
        elseif speed < self.m_min_speed then
            speed = self.m_min_speed
        end
        self.m_speed = speed
    end

    camera = FreeCamera.new(float3(0, 0, 0), float3(0, 0, -1), float3(1, 0, 0), float3(0, 1, 0), 1)

    function GetCameraPos()
        return camera.m_pos
    end

    function GetCameraUp()
        return camera.m_up
    end

    function GetCameraForward()
        return camera.m_forward
    end

    function MoveCamera(a_x, a_y, a_z)
        camera:Move(a_x, a_y, a_z)
    end

    function RotateCamera(a_x, a_y)
        camera:Rotate(a_x, a_y)
    end

    function AddSpeed(a_speed)
        camera:AddSpeed(a_speed)
    end

    function Update(a_delta_time, selected)
        camera:Update(a_delta_time)
    end
)";

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

    // create input
    {
        InputActionCreateInfo input_create{};
        input_create.value_type = INPUT_VALUE_TYPE::FLOAT_2;
        input_create.action_type = INPUT_ACTION_TYPE::BUTTON;
        input_create.binding_type = INPUT_BINDING_TYPE::COMPOSITE_UP_DOWN_RIGHT_LEFT;
        input_create.source = INPUT_SOURCE::KEYBOARD;
        input_create.input_keys[0].keyboard_key = KEYBOARD_KEY::W;
        input_create.input_keys[1].keyboard_key = KEYBOARD_KEY::S;
        input_create.input_keys[2].keyboard_key = KEYBOARD_KEY::D;
        input_create.input_keys[3].keyboard_key = KEYBOARD_KEY::A;
        m_move_forward_backward_left_right = Input::CreateInputAction("RenderViewport Move", input_create);
    }
    {
        InputActionCreateInfo input_create{};
        input_create.value_type = INPUT_VALUE_TYPE::FLOAT;
        input_create.action_type = INPUT_ACTION_TYPE::VALUE;
        input_create.binding_type = INPUT_BINDING_TYPE::BINDING;
        input_create.source = INPUT_SOURCE::MOUSE;
        input_create.input_keys[0].mouse_input = MOUSE_INPUT::SCROLL_WHEEL;
        m_move_speed_slider = Input::CreateInputAction("RenderViewport move speed slider", input_create);
    }
    {
        InputActionCreateInfo input_create{};
        input_create.value_type = INPUT_VALUE_TYPE::FLOAT_2;
        input_create.action_type = INPUT_ACTION_TYPE::VALUE;
        input_create.binding_type = INPUT_BINDING_TYPE::BINDING;
        input_create.source = INPUT_SOURCE::MOUSE;
        input_create.input_keys[0].mouse_input = MOUSE_INPUT::MOUSE_MOVE;
        m_look_around = Input::CreateInputAction("RenderViewport look around", input_create);
    }
    {
        InputActionCreateInfo input_create{};
        input_create.value_type = INPUT_VALUE_TYPE::BOOL;
        input_create.action_type = INPUT_ACTION_TYPE::VALUE;
        input_create.binding_type = INPUT_BINDING_TYPE::BINDING;
        input_create.source = INPUT_SOURCE::MOUSE;
        input_create.input_keys[0].mouse_input = MOUSE_INPUT::RIGHT_BUTTON;
        m_enable_rotate_button = Input::CreateInputAction("RenderViewport enable rotate", input_create);
    }

	m_viewport.Init(a_game_viewport_size, int2(0, 0), "render viewport");

    m_context.Init(m_memory, &m_scene_hierarchy.GetECS(), gbSize);

    int status = luaL_dostring(m_context.GetState(), lua_scene);
    BB_ASSERT(status == LUA_OK, lua_tostring(m_context.GetState(), -1));
	return true;
}

bool RenderViewport::Update(const float a_delta_time, const bool a_selected)
{
    const LuaStackScope scope(m_context.GetState());

    if (a_selected)
    {
        const float2 move_value = Input::InputActionGetFloat2(m_move_forward_backward_left_right);
        const float3 player_move = float3(move_value.x, 0.f, move_value.y) * a_delta_time;
        const float wheel_move = Input::InputActionGetFloat(m_move_speed_slider);
        const float speed = wheel_move;

        lua_getglobal(m_context.GetState(), "AddSpeed");
        lua_pushnumber(m_context.GetState(), static_cast<lua_Number>(speed));
        BB_ASSERT(lua_pcall(m_context.GetState(), 1, 0, 0) == LUA_OK, lua_tostring(m_context.GetState(), -1));

        lua_getglobal(m_context.GetState(), "MoveCamera");
        lua_pushnumber(m_context.GetState(), static_cast<lua_Number>(player_move.x));
        lua_pushnumber(m_context.GetState(), static_cast<lua_Number>(player_move.y));
        lua_pushnumber(m_context.GetState(), static_cast<lua_Number>(player_move.z));
        BB_ASSERT(lua_pcall(m_context.GetState(), 3, 0, 0) == LUA_OK, lua_tostring(m_context.GetState(), -1));

        if (Input::InputActionIsHeld(m_enable_rotate_button))
        {
            const float2 mouse_move = Input::InputActionGetFloat2(m_look_around) * a_delta_time;
            lua_getglobal(m_context.GetState(), "RotateCamera");
            lua_pushnumber(m_context.GetState(), static_cast<lua_Number>(mouse_move.x));
            lua_pushnumber(m_context.GetState(), static_cast<lua_Number>(mouse_move.y));
            BB_ASSERT(lua_pcall(m_context.GetState(), 2, 0, 0) == LUA_OK, lua_tostring(m_context.GetState(), -1));
        }
    }

    lua_getglobal(m_context.GetState(), "Update");
    lua_pushnumber(m_context.GetState(), static_cast<lua_Number>(a_delta_time));
    lua_pushboolean(m_context.GetState(), a_selected);
    BB_ASSERT(lua_pcall(m_context.GetState(), 2, 0, 0) == LUA_OK, lua_tostring(m_context.GetState(), -1));

    const float3 pos = GetCameraPos();
    lua_getglobal(m_context.GetState(), "GetCameraUp");
    BB_ASSERT(lua_pcall(m_context.GetState(), 0, 1, 0) == LUA_OK, lua_tostring(m_context.GetState(), -1));
    const float3 up = *lua_getfloat3(m_context.GetState(), -1);
    lua_getglobal(m_context.GetState(), "GetCameraForward");
    BB_ASSERT(lua_pcall(m_context.GetState(), 0, 1, 0) == LUA_OK, lua_tostring(m_context.GetState(), -1));
    const float3 forward = *lua_getfloat3(m_context.GetState(), -1);

    const float4x4 view = Float4x4Lookat(pos, pos + forward, up);

	m_scene_hierarchy.GetECS().GetRenderSystem().SetView(view, pos);
	DisplayImGuiInfo();

	return true;
}

float3 RenderViewport::GetCameraPos()
{
    const LuaStackScope scope(m_context.GetState());
    lua_getglobal(m_context.GetState(), "GetCameraPos");
    lua_pcall(m_context.GetState(), 0, 1, 0);
    return *lua_getfloat3(m_context.GetState(), -1);
}

#include "imgui.h"

void RenderViewport::DisplayImGuiInfo()
{
	if (ImGui::Begin("render viewport info"))
	{

	}
	ImGui::End();
}

void RenderViewport::Destroy()
{

}

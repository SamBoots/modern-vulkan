#include "JsonLoading.hpp"
#include "InputSystem.hpp"
#include "BBjson.hpp"
#include "SceneHierarchy.hpp"

using namespace BB;

InputChannelHandle BB::CreateInputChannelByJson(MemoryArena& a_arena, const StringView a_app_name, const StringView a_json_path)
{
    JsonParser parser(a_json_path.c_str());
    parser.Parse();
    const JsonList& ias = parser.GetRootNode()->GetObject().Find("input_actions")->GetList();

    const InputChannelHandle channel = Input::CreateInputChannel(a_arena, a_app_name, ias.node_count);
    for (uint32_t i = 0; i < ias.node_count; i++)
    {
        const JsonObject& iaobj = ias.nodes[i]->GetObject();
        InputActionCreateInfo create_info;
        InputActionName name = iaobj.Find("name")->GetString();
        create_info.value_type = STR_TO_INPUT_VALUE_TYPE(iaobj.Find("INPUT_VALUE")->GetString());
        create_info.binding_type = STR_TO_INPUT_BINDING_TYPE(iaobj.Find("INPUT_BINDING")->GetString());
        create_info.source = STR_TO_INPUT_SOURCE(iaobj.Find("INPUT_SOURCE")->GetString());

        const JsonList& keys = iaobj.Find("KEYS")->GetList();
        
        if (create_info.source == INPUT_SOURCE::KEYBOARD)
            for (uint32_t key_i = 0; key_i < keys.node_count; key_i++)
                create_info.input_keys[key_i].keyboard_key = STR_TO_KEYBOARD_KEY(keys.nodes[key_i]->GetString());
        else if (create_info.source == INPUT_SOURCE::MOUSE)
            for (uint32_t key_i = 0; key_i < keys.node_count; key_i++)
                create_info.input_keys[key_i].mouse_input = STR_TO_MOUSE_INPUT(keys.nodes[key_i]->GetString());

        const InputActionHandle ac = Input::CreateInputAction(channel, name, create_info);
        BB_ASSERT(ac.IsValid(), "input action is not valid");
    }
    return channel;
}

void BB::CreateEntitiesViaJson(SceneHierarchy& a_hierarchy, const StringView a_json_path)
{
    JsonParser parser(a_json_path.c_str());
    parser.Parse();
    const JsonObject& scene_obj = parser.GetRootNode()->GetObject().Find("scene")->GetObject();

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

#include "JsonLoading.hpp"
#include "InputSystem.hpp"
#include "BBjson.hpp"

using namespace BB;

InputChannelHandle BB::CreateInputChannelByJson(MemoryArena& a_arena, const StringView a_app_name, const PathString& a_json_path)
{
    const JsonParser parser(a_json_path.c_str());

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
        
        if (create_info.source == INPUT_SOURCE::KEYBOARD)
            create_info.input_keys[0].keyboard_key = STR_TO_KEYBOARD_KEY(iaobj.Find("KEY0")->GetString());
        else if (create_info.source == INPUT_SOURCE::MOUSE)
            create_info.input_keys[0].mouse_input = STR_TO_MOUSE_INPUT(iaobj.Find("KEY0")->GetString());

        if (create_info.binding_type == INPUT_BINDING_TYPE::COMPOSITE_UP_DOWN_RIGHT_LEFT)
        {
            create_info.input_keys[1].keyboard_key = STR_TO_KEYBOARD_KEY(iaobj.Find("KEY1")->GetString());
            create_info.input_keys[2].keyboard_key = STR_TO_KEYBOARD_KEY(iaobj.Find("KEY2")->GetString());
            create_info.input_keys[3].keyboard_key = STR_TO_KEYBOARD_KEY(iaobj.Find("KEY3")->GetString());
        }

        const InputActionHandle ac = Input::CreateInputAction(channel, name, create_info);
        BB_ASSERT(ac.IsValid(), "input action is not valid");
    }
    return channel;
}

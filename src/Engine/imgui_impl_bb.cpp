// dear imgui: Renderer Backend for CrossRenderer, referenced of the imgui backend base for Vulkan.

#include "imgui_impl_bb.hpp"
#include "Program.h"

#include "shared_common.hlsl.h"

using namespace BB;

constexpr size_t INITIAL_VERTEX_SIZE = sizeof(Vertex2D) * 128;
constexpr size_t INITIAL_INDEX_SIZE = sizeof(uint32_t) * 256;

// Reusable buffers used for rendering 1 current in-flight frame
struct ImGui_ImplBB_FrameRenderBuffers
{
    size_t vertex_size;
    size_t index_size;
    GPUBuffer vertex_buffer;
    GPUBuffer index_buffer;
    void* vertex_start;
    void* index_start;
};

struct ImGui_ImplBBRenderer_Data
{
    uint64_t                    buffer_memory_alignment;
    ShaderEffectHandle          vertex_shader;
    ShaderEffectHandle          fragment_shader;
    MaterialHandle              imgui_material;
    RTexture                    font_image;

    // Render buffers for main window
    uint32_t frame_index;
    ImGui_ImplBB_FrameRenderBuffers* frame_buffers;

    //follow imgui convention from different imgui source files.
    ImGui_ImplBBRenderer_Data()
    {
        memset((void*)this, 0, sizeof(*this));
        buffer_memory_alignment = 256;
    }
};

struct ImGui_ImplBB_Data
{
    BB::WindowHandle            window;
    int                         MouseTrackedArea;   // 0: not tracked, 1: client are, 2: non-client area
    int                         MouseButtonsDown;
    int64_t                     Time;
    int64_t                     TicksPerSecond;
    ImGuiMouseCursor            LastMouseCursor;

    ImGui_ImplBB_Data() { memset((void*)this, 0, sizeof(*this)); }
};

//-----------------------------------------------------------------------------
// FUNCTIONS
//-----------------------------------------------------------------------------

static ImGui_ImplBBRenderer_Data* ImGui_ImplCross_GetBackendData()
{
    return ImGui::GetCurrentContext() ? reinterpret_cast<ImGui_ImplBBRenderer_Data*>(ImGui::GetIO().BackendRendererUserData) : nullptr;
}

static ImGui_ImplBB_Data* ImGui_ImplBB_GetPlatformData()
{
    return ImGui::GetCurrentContext() ? reinterpret_cast<ImGui_ImplBB_Data*>(ImGui::GetIO().BackendPlatformUserData) : nullptr;
}

static void ImGui_ImplCross_SetupRenderState(const ImDrawData& a_DrawData, 
    const RCommandList a_cmd_lists,
    const ImGui_ImplBB_FrameRenderBuffers& a_render_buffers, 
    const int a_fb_width, 
    const int a_fb_height)
{
    ImGui_ImplBBRenderer_Data* bd = ImGui_ImplCross_GetBackendData();

    // Setup scale and translation:
    // Our visible imgui space lies from draw_data->DisplayPps (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right). DisplayPos is (0,0) for single viewport apps.
    {
        float scale[2];
        scale[0] = 2.0f / a_DrawData.DisplaySize.x;
        scale[1] = 2.0f / a_DrawData.DisplaySize.y;
        float translate[2];
        translate[0] = -1.0f - a_DrawData.DisplayPos.x * scale[0];
        translate[1] = -1.0f - a_DrawData.DisplayPos.y * scale[1];
        uint32_t texture_index = bd->font_image.index;
        //Constant index will always be 0 if we use it. Imgui pipeline will always use it.
        uint32_t t_Offset = 0;
        RenderBackend::BindConstant(a_CmdList, 0, _countof(scale), t_Offset, &scale);
        t_Offset += _countof(scale);
        RenderBackend::BindConstant(a_CmdList, 0, _countof(translate), t_Offset, &translate);
    }
}

static void GrowFrameBufferGPUBuffers(ImGui_ImplBB_FrameRenderBuffers& a_rb, const size_t a_new_vertex_size, const size_t a_new_index_size)
{
    UnmapGPUBuffer(a_rb.vertex_buffer);
    UnmapGPUBuffer(a_rb.index_buffer);

    FreeGPUBuffer(a_rb.vertex_buffer);
    FreeGPUBuffer(a_rb.index_buffer);

    a_rb.vertex_size = a_new_vertex_size;
    a_rb.index_size = a_new_index_size;

    GPUBufferCreateInfo vertex_buffer;
    vertex_buffer.name = "imgui vertex buffer";
    vertex_buffer.type = BUFFER_TYPE::STORAGE;
    vertex_buffer.size = a_rb.vertex_size;
    vertex_buffer.host_writable = true;
    a_rb.vertex_buffer = CreateGPUBuffer(vertex_buffer);
    a_rb.vertex_start = MapGPUBuffer(a_rb.vertex_buffer);

    GPUBufferCreateInfo index_buffer;
    index_buffer.name = "imgui index buffer";
    index_buffer.type = BUFFER_TYPE::INDEX;
    index_buffer.size = a_rb.index_size;
    index_buffer.host_writable = true;
    a_rb.index_buffer = CreateGPUBuffer(index_buffer);
    a_rb.index_start = MapGPUBuffer(a_rb.index_buffer);
}

// Render function
void BB::ImGui_ImplBB_RenderDrawData(const ImDrawData& a_DrawData, const RCommandList a_cmd_list)
{
    // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
    int fb_width = (int)(a_DrawData.DisplaySize.x * a_DrawData.FramebufferScale.x);
    int fb_height = (int)(a_DrawData.DisplaySize.y * a_DrawData.FramebufferScale.y);
    if (fb_width <= 0 || fb_height <= 0)
        return;

    ImGui_ImplBBRenderer_Data* bd = ImGui_ImplCross_GetBackendData();
    const RenderIO render_io = GetRenderIO();

    BB_ASSERT(bd->frame_index < render_io.frame_count, "Frame index is higher then the framebuffer amount! Forgot to resize the imgui window info.");
    bd->frame_index = (bd->frame_index + 1) % render_io.frame_count;
    ImGui_ImplBB_FrameRenderBuffers& rb = bd->frame_buffers[bd->frame_index];

    if (a_DrawData.TotalVtxCount > 0)
    {
        // Create or resize the vertex/index buffers
        const size_t vertex_size = a_DrawData.TotalVtxCount * sizeof(ImDrawVert);
        const size_t index_size = a_DrawData.TotalIdxCount * sizeof(ImDrawIdx);
        if (rb.vertex_size < vertex_size || rb.index_size < index_size)
            GrowFrameBufferGPUBuffers(rb, Max(rb.vertex_size * 2, vertex_size), Max(rb.index_size * 2, index_size));

        // Upload vertex/index data into a single contiguous GPU buffer
        ImDrawVert* vtx_dst = reinterpret_cast<ImDrawVert*>(rb.vertex_start);
        ImDrawIdx* idx_dst = reinterpret_cast<ImDrawIdx*>(rb.index_start);
       
        for (int n = 0; n < a_DrawData.CmdListsCount; n++)
        {
            const ImDrawList* cmd_list = a_DrawData.CmdLists[n];
            memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
            memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
            vtx_dst += cmd_list->VtxBuffer.Size;
            idx_dst += cmd_list->IdxBuffer.Size;
        }
    }

    StartRenderingInfo imgui_pass_start{};
    imgui_pass_start.viewport_width = static_cast<uint32_t>(fb_width);
    imgui_pass_start.viewport_height = static_cast<uint32_t>(fb_height);
    imgui_pass_start.load_color = true;
    imgui_pass_start.store_color = true;
    imgui_pass_start.initial_layout = IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL;
    imgui_pass_start.final_layout = IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL;
    StartRenderPass(a_cmd_list, imgui_pass_start);

    // Setup desired CrossRenderer state
    ImGui_ImplCross_SetupRenderState(a_DrawData, a_cmd_list, rb, fb_width, fb_height);

    // Will project scissor/clipping rectangles into framebuffer space
    ImVec2 clip_off = a_DrawData.DisplayPos;         // (0,0) unless using multi-viewports
    ImVec2 clip_scale = a_DrawData.FramebufferScale; // (1,1) unless using retina display which are often (2,2)

    // Render command lists
    // (Because we merged all buffers into a single one, we maintain our own offset into them)
    int global_vtx_offset = 0;
    int global_idx_offset = 0;
    for (int n = 0; n < a_DrawData.CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = a_DrawData.CmdLists[n];
        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback != nullptr)
            {
                // User callback, registered via ImDrawList::AddCallback()
                // (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
                if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
                    ImGui_ImplCross_SetupRenderState(a_DrawData, a_cmd_list, rb, fb_width, fb_height);
                else
                    pcmd->UserCallback(cmd_list, pcmd);
            }
            else
            {
                // Project scissor/clipping rectangles into framebuffer space
                ImVec2 clip_min(pcmd->ClipRect.x - clip_off.x, pcmd->ClipRect.y - clip_off.y);
                ImVec2 clip_max(pcmd->ClipRect.z - clip_off.x, pcmd->ClipRect.w - clip_off.y);

                // Clamp to viewport as vkCmdSetScissor() won't accept values that are off bounds
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                    continue;
                
                //offset = 2 vec2's. So 4 dwords
                RenderBackend::BindConstant(a_CmdList, 0, 1, 4, &pcmd->TextureId);
                // Apply scissor/clipping rectangle
                ScissorInfo scissor;
                scissor.offset.x = (int32_t)(clip_min.x);
                scissor.offset.y = (int32_t)(clip_min.y);
                scissor.extent.x = (uint32_t)(clip_max.x);
                scissor.extent.y = (uint32_t)(clip_max.y);
                SetScissor(a_cmd_list, scissor);

                // Draw
                RenderBackend::DrawIndexed(a_CmdList, pcmd->ElemCount, 1, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset, 0);
            }
        }
        global_idx_offset += cmd_list->IdxBuffer.Size;
        global_vtx_offset += cmd_list->VtxBuffer.Size;
    }

    // Since we dynamically set our scissor lets set it back to the full viewport. 
    // This might be bad to do since this can leak into different system's code. 
    ScissorInfo scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = { static_cast<uint32_t>(fb_width), static_cast<uint32_t>(fb_height) };
    SetScissor(a_cmd_list, scissor);

    EndRenderingInfo imgui_pass_end;
    imgui_pass_end.initial_layout = imgui_pass_start.final_layout;
    imgui_pass_end.final_layout = IMAGE_LAYOUT::PRESENT;
    EndRenderPass(a_cmd_list, imgui_pass_end);
}

bool BB::ImGui_ImplBB_CreateFontsTexture(const RCommandList a_cmd_list, UploadBufferView& a_upload_view)
{
    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplBBRenderer_Data* bd = ImGui_ImplCross_GetBackendData();

    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    size_t upload_size = static_cast<size_t>(width) * height * 4;

    UploadImageInfo font_info;
    font_info.name = "imgui font";
    font_info.width = static_cast<uint32_t>(width);
    font_info.height = static_cast<uint32_t>(height);
    font_info.bit_count = 32;
    font_info.pixels = pixels;
    bd->font_image = UploadTexture(a_cmd_list, font_info, a_upload_view);

    io.Fonts->SetTexID(bd->font_image.handle);

    return bd->font_image.IsValid();
}

void ImGui_ImplCross_DestroyFontUploadObjects()
{
    ImGui_ImplBBRenderer_Data* bd = ImGui_ImplCross_GetBackendData();
    FreeTexture(bd->font_image);
    bd->font_image = RTexture(BB_INVALID_HANDLE_32);
}

bool ImGui_ImplCross_Init(Allocator a_temp_allocator, const ImGui_ImplBB_InitInfo& a_info)
{
    ImGuiIO& io = ImGui::GetIO();
    IM_ASSERT(io.BackendRendererUserData == nullptr && "Already initialized a renderer backend!");

    { // WIN implementation
        // Setup backend capabilities flags
        ImGui_ImplBB_Data* bdWin = IM_NEW(ImGui_ImplBB_Data)();
        io.BackendPlatformUserData = (void*)bdWin;
        io.BackendPlatformName = "imgui_impl_BB";
        io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;         // We can honor GetMouseCursor() values (optional)
        io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;          // We can honor io.WantSetMousePos requests (optional, rarely used)

        bdWin->window = a_info.window;
        //bd->TicksPerSecond = perf_frequency;
        //bd->Time = perf_counter;
        bdWin->LastMouseCursor = ImGuiMouseCursor_COUNT;

        // Set platform dependent data in viewport
        ImGui::GetMainViewport()->PlatformHandleRaw = reinterpret_cast<void*>(a_info.window.handle);
    }
    // Setup backend capabilities flags
    ImGui_ImplBBRenderer_Data* bd = IM_NEW(ImGui_ImplBBRenderer_Data)();
    io.BackendRendererUserData = (void*)bd;
    io.BackendRendererName = "imgui_impl_crossrenderer";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;  // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.

    IM_ASSERT(a_info.min_image_count >= 2);
    IM_ASSERT(a_info.image_count >= a_info.min_image_count);

    {
        CreateShaderEffectInfo shaders[2];
        shaders[0].name = "imgui vertex shader";
        shaders[0].shader_path = "../resources/shaders/hlsl/Imgui.hlsl";
        shaders[0].shader_entry = "VertexMain";
        shaders[0].stage = SHADER_STAGE::VERTEX;
        shaders[0].next_stages = static_cast<SHADER_STAGE_FLAGS>(SHADER_STAGE::FRAGMENT_PIXEL);
        shaders[0].push_constant_space = sizeof(ShaderIndices2D);

        shaders[1].name = "imgui Fragment shader";
        shaders[1].shader_path = "../resources/shaders/hlsl/Imgui.hlsl";
        shaders[1].shader_entry = "FragmentMain";
        shaders[1].stage = SHADER_STAGE::FRAGMENT_PIXEL;
        shaders[1].next_stages = static_cast<SHADER_STAGE_FLAGS>(SHADER_STAGE::NONE);
        shaders[1].push_constant_space = sizeof(ShaderIndices2D);

        ShaderEffectHandle shader_objects[2];
        BB_ASSERT(CreateShaderEffect(a_temp_allocator, Slice(shaders, _countof(shaders)), shader_objects),
            "Failed to create imgui shaders");
        bd->vertex_shader = shader_objects[0];
        bd->fragment_shader = shader_objects[1];
    }

    //create framebuffers.
    {
        const RenderIO render_io = GetRenderIO();
        bd->frame_index = 0;
        bd->frame_buffers = (ImGui_ImplBB_FrameRenderBuffers*)IM_ALLOC(sizeof(ImGui_ImplBB_FrameRenderBuffers) * render_io.frame_count);

        for (size_t i = 0; i < render_io.frame_count; i++)
        {
            //I love C++
            new (&bd->frame_buffers[i])(ImGui_ImplBB_FrameRenderBuffers);
            ImGui_ImplBB_FrameRenderBuffers& rb = bd->frame_buffers[i];
            rb.vertex_size = INITIAL_VERTEX_SIZE;
            rb.index_size = INITIAL_INDEX_SIZE;

            GPUBufferCreateInfo vertex_buffer;
            vertex_buffer.name = "imgui vertex buffer";
            vertex_buffer.type = BUFFER_TYPE::STORAGE;
            vertex_buffer.size = rb.vertex_size;
            vertex_buffer.host_writable = true;
            rb.vertex_buffer = CreateGPUBuffer(vertex_buffer);
            rb.vertex_start = MapGPUBuffer(rb.vertex_buffer);

            GPUBufferCreateInfo index_buffer;
            index_buffer.name = "imgui index buffer";
            index_buffer.type = BUFFER_TYPE::INDEX;
            index_buffer.size = rb.index_size;
            index_buffer.host_writable = true;
            rb.index_buffer = CreateGPUBuffer(index_buffer);
            rb.index_start = MapGPUBuffer(rb.index_buffer);
        }
    }

    return true;
}

void ImGui_ImplCross_Shutdown()
{
    ImGui_ImplBBRenderer_Data* bd = ImGui_ImplCross_GetBackendData();
    IM_ASSERT(bd != nullptr && "No renderer backend to shutdown, or already shutdown?");
    ImGuiIO& io = ImGui::GetIO();

    //delete my things here.

    ImGui_ImplBB_Data* pd = ImGui_ImplBB_GetPlatformData();
    IM_ASSERT(pd != nullptr && "No platform backend to shutdown, or already shutdown?");

    io.BackendPlatformName = nullptr;
    io.BackendPlatformUserData = nullptr;
    IM_DELETE(bd);
    IM_DELETE(pd);
}

void ImGui_ImplCross_NewFrame()
{
    ImGui_ImplBB_Data* bd = ImGui_ImplBB_GetPlatformData();
    IM_ASSERT(bd != nullptr && "Did you call ImGui_ImplCross_Init()?");
    ImGuiIO& io = ImGui::GetIO();

    int x, y;
    GetWindowSize(bd->window, x, y);
    io.DisplaySize = ImVec2((float)(x), (float)(y));

    IM_UNUSED(bd);
}

//BB FRAMEWORK TEMPLATE, MAY CHANGE THIS.
static ImGuiKey ImGui_ImplBB_KEYBOARD_KEYToImGuiKey(const KEYBOARD_KEY a_Key)
{
    switch (a_Key)
    {
    case KEYBOARD_KEY::TAB: return ImGuiKey_Tab;
    case KEYBOARD_KEY::BACKSPACE: return ImGuiKey_Backspace;
    case KEYBOARD_KEY::SPACEBAR: return ImGuiKey_Space;
    case KEYBOARD_KEY::RETURN: return ImGuiKey_Enter;
    case KEYBOARD_KEY::ESCAPE: return ImGuiKey_Escape;
    case KEYBOARD_KEY::APOSTROPHE: return ImGuiKey_Apostrophe;
    case KEYBOARD_KEY::COMMA: return ImGuiKey_Comma;
    case KEYBOARD_KEY::MINUS: return ImGuiKey_Minus;
    case KEYBOARD_KEY::PERIOD: return ImGuiKey_Period;
    case KEYBOARD_KEY::SLASH: return ImGuiKey_Slash;
    case KEYBOARD_KEY::SEMICOLON: return ImGuiKey_Semicolon;
    case KEYBOARD_KEY::EQUALS: return ImGuiKey_Equal;
    case KEYBOARD_KEY::BRACKETLEFT: return ImGuiKey_LeftBracket;
    case KEYBOARD_KEY::BACKSLASH: return ImGuiKey_Backslash;
    case KEYBOARD_KEY::BRACKETRIGHT: return ImGuiKey_RightBracket;
    case KEYBOARD_KEY::GRAVE: return ImGuiKey_GraveAccent;
    case KEYBOARD_KEY::CAPSLOCK: return ImGuiKey_CapsLock;
    case KEYBOARD_KEY::NUMPADMULTIPLY: return ImGuiKey_KeypadMultiply;
    case KEYBOARD_KEY::SHIFTLEFT: return ImGuiKey_LeftShift;
    case KEYBOARD_KEY::CONTROLLEFT: return ImGuiKey_LeftCtrl;
    case KEYBOARD_KEY::ALTLEFT: return ImGuiKey_LeftAlt;
    case KEYBOARD_KEY::SHIFTRIGHT: return ImGuiKey_RightShift;
    case KEYBOARD_KEY::KEY_0: return ImGuiKey_0;
    case KEYBOARD_KEY::KEY_1: return ImGuiKey_1;
    case KEYBOARD_KEY::KEY_2: return ImGuiKey_2;
    case KEYBOARD_KEY::KEY_3: return ImGuiKey_3;
    case KEYBOARD_KEY::KEY_4: return ImGuiKey_4;
    case KEYBOARD_KEY::KEY_5: return ImGuiKey_5;
    case KEYBOARD_KEY::KEY_6: return ImGuiKey_6;
    case KEYBOARD_KEY::KEY_7: return ImGuiKey_7;
    case KEYBOARD_KEY::KEY_8: return ImGuiKey_8;
    case KEYBOARD_KEY::KEY_9: return ImGuiKey_9;
    case KEYBOARD_KEY::A: return ImGuiKey_A;
    case KEYBOARD_KEY::B: return ImGuiKey_B;
    case KEYBOARD_KEY::C: return ImGuiKey_C;
    case KEYBOARD_KEY::D: return ImGuiKey_D;
    case KEYBOARD_KEY::E: return ImGuiKey_E;
    case KEYBOARD_KEY::F: return ImGuiKey_F;
    case KEYBOARD_KEY::G: return ImGuiKey_G;
    case KEYBOARD_KEY::H: return ImGuiKey_H;
    case KEYBOARD_KEY::I: return ImGuiKey_I;
    case KEYBOARD_KEY::J: return ImGuiKey_J;
    case KEYBOARD_KEY::K: return ImGuiKey_K;
    case KEYBOARD_KEY::L: return ImGuiKey_L;
    case KEYBOARD_KEY::M: return ImGuiKey_M;
    case KEYBOARD_KEY::N: return ImGuiKey_N;
    case KEYBOARD_KEY::O: return ImGuiKey_O;
    case KEYBOARD_KEY::P: return ImGuiKey_P;
    case KEYBOARD_KEY::Q: return ImGuiKey_Q;
    case KEYBOARD_KEY::R: return ImGuiKey_R;
    case KEYBOARD_KEY::S: return ImGuiKey_S;
    case KEYBOARD_KEY::T: return ImGuiKey_T;
    case KEYBOARD_KEY::U: return ImGuiKey_U;
    case KEYBOARD_KEY::V: return ImGuiKey_V;
    case KEYBOARD_KEY::W: return ImGuiKey_W;
    case KEYBOARD_KEY::X: return ImGuiKey_X;
    case KEYBOARD_KEY::Y: return ImGuiKey_Y;
    case KEYBOARD_KEY::Z: return ImGuiKey_Z;
    default: return ImGuiKey_None;
    }
}

//On true means that imgui takes the input and doesn't give it to the engine.
bool ImGui_ImplCross_ProcessInput(const BB::InputEvent& a_input_event)
{
    ImGuiIO& io = ImGui::GetIO();
    if (a_input_event.input_type == INPUT_TYPE::MOUSE)
    {
        const BB::MouseInfo& mouse_info = a_input_event.mouse_info;
        io.AddMousePosEvent(mouse_info.mouse_pos.x, mouse_info.mouse_pos.y);
        if (a_input_event.mouse_info.wheel_move != 0)
        {
            io.AddMouseWheelEvent(0.0f, (float)a_input_event.mouse_info.wheel_move);
        }

        constexpr int left_button = 0;
        constexpr int right_button = 1;
        constexpr int middle_button = 2;

        if (mouse_info.left_pressed)
            io.AddMouseButtonEvent(left_button, true);
        if (mouse_info.right_pressed)
            io.AddMouseButtonEvent(right_button, true);
        if (mouse_info.middle_pressed)
            io.AddMouseButtonEvent(middle_button, true);

        if (mouse_info.left_released)
            io.AddMouseButtonEvent(left_button, false);
        if (mouse_info.right_released)
            io.AddMouseButtonEvent(right_button, false);
        if (mouse_info.middle_released)
            io.AddMouseButtonEvent(middle_button, false);

        return io.WantCaptureMouse;
    }
    else if (a_input_event.input_type == INPUT_TYPE::KEYBOARD)
    {
        const BB::KeyInfo& key_info = a_input_event.key_info;
        const ImGuiKey imgui_key = ImGui_ImplBB_KEYBOARD_KEYToImGuiKey(key_info.scan_code);

        io.AddKeyEvent(imgui_key, key_info.key_pressed);
        //THIS IS WRONG! It gives no UTF16 character.
        //But i'll keep it in here to test if imgui input actually works.
        io.AddInputCharacterUTF16((ImWchar16)key_info.scan_code);
        //We want unused warnings.
        //IM_UNUSED(t_ImguiKey);

        return io.WantCaptureKeyboard;
    }

    return false;
}
#include "LineStage.hpp"
#include "Renderer.hpp"
#include "MaterialSystem.hpp"

using namespace BB;

constexpr uint32_t INITIAL_DEPTH_ARRAY_COUNT = 8;

void LineStage::Init(MemoryArena& a_arena, const uint32_t a_back_buffer_count, const uint32_t a_line_max)
{
    // todo shader

    m_per_frame.Init(a_arena, a_back_buffer_count);
    m_per_frame.resize(a_back_buffer_count);
    for (uint32_t i = 0; i < m_per_frame.size(); i++)
    {
        PerFrame& pfd = m_per_frame[i];
        pfd.vertex_view = AllocateFromWritableVertexBuffer(a_line_max * sizeof(Line));
    }
}

void LineStage::ExecutePass(const RCommandList a_list, const uint32_t a_frame_index, const ConstSlice<Line> a_lines)
{
    if (a_lines.size() == 0)
        return;

    PerFrame& pfd = m_per_frame[a_frame_index];

    size_t upload_size = a_lines.sizeInBytes();
    if (upload_size > pfd.vertex_view.size)
    {
        upload_size = pfd.vertex_view.size;
        BB_WARNING(false, "uploading too many lines, not all may be shown", WarningType::LOW);
    }
    memcpy(pfd.vertex_view.mapped, a_lines.data(), upload_size);
}

#pragma once
#include "GPUBuffers.hpp"
#include "Rendererfwd.hpp"

namespace BB
{
    class RenderSystem2D
    {
    public:
        bool Init(MemoryArena& a_arena, const size_t a_gpu_buffer);
        void Destroy();
        void InitFontImage(MemoryArena& a_temp_arena, const RCommandList a_list, GPUUploadRingAllocator& a_ring_buffer, const GPUFenceValue a_frame_fence_value, const PathString& a_font_path);

        void RenderText(const RCommandList a_list, const uint2 a_text_size, const uint2 a_text_start_pos, const StringView a_string);

    private:
        uint2 m_image_size;
        RDescriptorIndex m_font_atlas;
        GPUBuffer m_gpu_memory;
        AssetHandle m_image_asset;

        MasterMaterialHandle m_font_material;
    };
}

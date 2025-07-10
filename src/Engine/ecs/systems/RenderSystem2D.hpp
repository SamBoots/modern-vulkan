#pragma once
#include "GPUBuffers.hpp"
#include "Rendererfwd.hpp"
#include "Enginefwd.hpp"

namespace BB
{

    class RenderSystem2D
    {
    public:
        bool Init(MemoryArena& a_arena, const size_t a_max_glyphs_per_frame, const PathString& a_font_path);
        void Destroy();

        void RenderText(const RCommandList a_list, GPUUploadRingAllocator& a_ring_buffer, const GPULinearBuffer& a_frame_buffer, const uint2 a_text_size, const uint2 a_text_start_pos, const StringView a_string);

    private:
        uint2 m_image_size;
        RDescriptorIndex m_font_atlas;
        AssetHandle m_image_asset;

        StaticArray<Glyph2D> m_glyps;
        StaticArray<FixedArray<float2, 2>> m_uvs;

        MasterMaterialHandle m_material;
    };
}

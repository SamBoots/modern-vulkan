#include "RenderSystem2D.hpp"
#include "Rendererfwd.hpp"
#include "Renderer.hpp"

#include "AssetLoader.hpp"

using namespace BB;

bool RenderSystem2D::Init(MemoryArena& a_arena, const size_t a_gpu_buffer)
{

}

void RenderSystem2D::InitFontImage(MemoryArena& a_temp_arena, const RCommandList a_list, GPUUploadRingAllocator& a_ring_buffer, const GPUFenceValue a_frame_fence_value, const PathString& a_font_path)
{
    int width, height, bytes_per_pixel;
    unsigned char* pixels = Asset::LoadImageCPU(a_font_path.c_str(), width, height, bytes_per_pixel);

    Asset::TextureLoadFromMemory texture_load;
    texture_load.name = "font image";
    texture_load.width = width;
    texture_load.height = height;
    texture_load.pixels = pixels;
    texture_load.bytes_per_pixel = bytes_per_pixel;
    Asset::LoadImageMemory(a_temp_arena, texture_load);

    m_image_size = uint2(static_cast<uint32_t>(width), static_cast<uint32_t>(height));

    ConstSlice<GlyphUV> uvs = {};

    GPUBufferCreateInfo buffer_info;
    buffer_info.name = "glyph uv buffer";
    buffer_info.size = uvs.sizeInBytes();
    buffer_info.type = BUFFER_TYPE::UNIFORM;
    buffer_info.host_writable = false;
    m_gpu_memory = CreateGPUBuffer(buffer_info);

    const uint64_t buffer_start = a_ring_buffer.AllocateUploadMemory(uvs.sizeInBytes(), a_frame_fence_value);

    RenderCopyBufferRegion region;
    region.size = uvs.sizeInBytes();
    region.src_offset = buffer_start;
    region.dst_offset = 0;

    RenderCopyBuffer copy_op;
    copy_op.src = a_ring_buffer.GetBuffer();
    copy_op.dst = m_gpu_memory;
    copy_op.regions = Slice(&region, 1);
    CopyBuffer(a_list, copy_op);

    Asset::FreeImageCPU(pixels);
}

void RenderSystem2D::Destroy()
{
    Asset::FreeAsset()
}

void RenderSystem2D::RenderText(const RCommandList a_list, const uint2 a_text_size, const uint2 a_text_start_pos, const StringView a_string)
{

}

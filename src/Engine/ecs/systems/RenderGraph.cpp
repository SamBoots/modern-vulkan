#include "Rendergraph.hpp"
#include "Renderer.hpp"

using namespace BB;

RG::RenderPass::RenderPass(MemoryArena& a_arena, const PFN_RenderPass a_call, const uint32_t a_resources_in, const uint32_t a_resources_out, const MasterMaterialHandle a_material)
{
    m_call = a_call;
    m_material = a_material;
    m_resource_inputs.Init(a_arena, a_resources_in);
    m_resource_outputs.Init(a_arena, a_resources_out);
}

bool RG::RenderPass::DoPass(class RenderGraph& a_graph, GlobalGraphData& a_global_data, const RCommandList a_list)
{
    return m_call(a_graph, a_global_data, a_list, m_material, m_resource_inputs.slice(), m_resource_outputs.slice());
}

bool RG::RenderPass::AddInputResource(const ResourceHandle a_resource_index)
{
    if (m_resource_inputs.IsFull())
        return false;
    m_resource_inputs.emplace_back(a_resource_index);
    return true;
}

bool RG::RenderPass::AddOutputResource(const ResourceHandle a_resource_index)
{
    if (m_resource_outputs.IsFull())
        return false;
    m_resource_outputs.emplace_back(a_resource_index);
    return true;
}

RG::RenderGraph::RenderGraph(MemoryArena& a_arena, const uint32_t a_max_passes, const uint32_t a_max_resources)
{
    m_passes.Init(a_arena, a_max_passes);
    m_execution_order.Init(a_arena, a_max_passes);
    m_resources.Init(a_arena, a_max_resources);

    m_per_frame_descriptor = AllocateBufferDescriptor();
    m_scene_buffer.Init(BUFFER_TYPE::UNIFORM, sizeof(Scene3DInfo), "scene info buffer");
}

bool RG::RenderGraph::Reset()
{
    m_passes.clear();
    m_execution_order.clear();
    m_resources.clear();
    m_per_frame_buffer.Clear();

    // delete resources here
    return true;
}

bool RG::RenderGraph::Compile(MemoryArena& a_temp_arena, GPUUploadRingAllocator& a_upload_buffer, const uint64_t a_fence_value)
{
    for (uint32_t i = 0; i < m_execution_order.size(); i++)
    {
        m_execution_order[i] = i;
    }

    uint32_t pf_upload_region_count = 0;
    size_t pfd_buffer_size = 0;
    uint32_t image_uploads = 0;
    for (size_t i = 0; i < m_resources.size(); i++)
    {
        const RenderResource& res = m_resources[i];
        if (res.upload_data)
        {
            if (res.descriptor_type == DESCRIPTOR_TYPE::READONLY_BUFFER)
            {
                pfd_buffer_size += res.buffer.size;
                ++pf_upload_region_count;
            }
            else if (res.descriptor_type == DESCRIPTOR_TYPE::IMAGE)
            {
                ++image_uploads;
            }
        }
    }

    if (pfd_buffer_size > m_per_frame_buffer.GetCapacity())
    {   // buffer gen
        if (m_per_frame_buffer.GetCapacity() > 0)
            m_per_frame_buffer.Destroy();

        GPUBufferCreateInfo per_frame_create_info;
        per_frame_create_info.name = "per frame buffer";
        per_frame_create_info.size = pfd_buffer_size;
        per_frame_create_info.type = BUFFER_TYPE::STORAGE;
        per_frame_create_info.host_writable = false;
        m_per_frame_buffer.Init(per_frame_create_info);

        GPUBufferView per_frame_view;
        per_frame_view.buffer = m_per_frame_buffer.GetBuffer();
        per_frame_view.size = pfd_buffer_size;
        per_frame_view.offset = 0;
        DescriptorWriteStorageBuffer(m_per_frame_descriptor, per_frame_view);
    }

    RenderCopyBufferRegion* pf_regions = ArenaAllocArr(a_temp_arena, RenderCopyBufferRegion, pf_upload_region_count);
    uint32_t pf_current_region = 0;
    RenderCopyBufferToImageInfo* image_copies = ArenaAllocArr(a_temp_arena, RenderCopyBufferToImageInfo, image_uploads);
    uint32_t current_image_copy = 0;
    bool success = true;

    // upload all data
    for (size_t i = 0; i < m_resources.size(); i++)
    {
        if (success == false)
            return false;

        RenderResource& res = m_resources[i];
        if (res.descriptor_type == DESCRIPTOR_TYPE::READONLY_CONSTANT)
        {
            BB_ASSERT(false, "don't think I use this yet, implement when hitting this");
        }
        else if (res.descriptor_type == DESCRIPTOR_TYPE::READONLY_BUFFER)
        {
            success = m_per_frame_buffer.Allocate(res.buffer.size, res.buffer);
            if (res.upload_data)
            {
                const uint64_t src_offset = a_upload_buffer.AllocateUploadMemory(res.buffer.size, a_fence_value);
                if (src_offset == size_t(-1))
                {
                    success = false;
                    continue;
                }
 
                if (a_upload_buffer.MemcpyIntoBuffer(src_offset, res.upload_data, res.buffer.size) == false)
                    success = false;

                pf_regions[pf_current_region].size = res.buffer.size;
                pf_regions[pf_current_region].src_offset = src_offset;
                pf_regions[pf_current_region].dst_offset = res.buffer.offset;
                ++pf_current_region;
            }
        }
        else if (res.descriptor_type == DESCRIPTOR_TYPE::IMAGE)
        {
            ImageCreateInfo image_info;
            image_info.name = res.name.c_str();
            image_info.width = res.image.extent.x;
            image_info.height = res.image.extent.y;
            image_info.depth = res.image.extent.z;
            image_info.array_layers = res.image.array_layers;
            image_info.mip_levels = res.image.mips;
            if (res.image.extent.x == 1)
                image_info.type = IMAGE_TYPE::TYPE_2D;
            else
                image_info.type = IMAGE_TYPE::TYPE_3D;
            image_info.format = res.image.format;
            image_info.usage = res.image.usage;
            image_info.use_optimal_tiling = true;
            image_info.is_cube_map = false;

            res.image.image = CreateImage(image_info);
            ImageViewCreateInfo view_info;
            view_info.name = res.name.c_str();
            view_info.array_layers = res.image.array_layers;
            view_info.mip_levels = res.image.mips;
            view_info.base_array_layer = 0;
            view_info.base_mip_level = 0;

            if (res.image.extent.x == 1)
                if (res.image.array_layers == 1)
                    view_info.type = IMAGE_VIEW_TYPE::TYPE_2D;
                else
                    view_info.type = IMAGE_VIEW_TYPE::TYPE_2D_ARRAY;
            else
                view_info.type = IMAGE_VIEW_TYPE::TYPE_3D;

            view_info.format = res.image.format;
            if (res.image.usage == IMAGE_USAGE::DEPTH || res.image.usage == IMAGE_USAGE::SHADOW_MAP)
                view_info.aspects = IMAGE_ASPECT::COLOR;
            else
                view_info.aspects = IMAGE_ASPECT::DEPTH;

            res.descriptor_index = CreateImageView(view_info);

            if (res.upload_data)
            {
                const uint64_t pixel_byte_size = GetPixelByteSizeImageFormat(res.image.format);
                const uint64_t upload_size = static_cast<uint64_t>(res.image.extent.x * res.image.extent.y * res.image.extent.z * pixel_byte_size);
                const uint64_t src_offset = a_upload_buffer.AllocateUploadMemory(upload_size, a_fence_value);
                if (src_offset == size_t(-1))
                {
                    success = false;
                    continue;
                }
                
                if (a_upload_buffer.MemcpyIntoBuffer(src_offset, res.upload_data, upload_size) == 0)
                    success = false;

                image_copies[current_image_copy].src_buffer = a_upload_buffer.GetBuffer();
                image_copies[current_image_copy].src_offset = src_offset;
                image_copies[current_image_copy].dst_image = res.image.image;
                image_copies[current_image_copy].dst_aspects = view_info.aspects;
                image_copies[current_image_copy].dst_image_info.base_array_layer = 0;
                image_copies[current_image_copy].dst_image_info.extent = res.image.extent;
                image_copies[current_image_copy].dst_image_info.layer_count = res.image.array_layers;
                image_copies[current_image_copy].dst_image_info.mip_level = 0;
                image_copies[current_image_copy].dst_image_info.offset = int3(0, 0, 0);
                ++current_image_copy;
            }
        }
        else if (res.descriptor_type == DESCRIPTOR_TYPE::SAMPLER)
        {
            // should be handled
        }
    }

    return true;
}

RG::RenderPass& RG::RenderGraph::AddRenderPass(MemoryArena& a_arena, const PFN_RenderPass a_call, const uint32_t a_resources_in, const uint32_t a_resources_out, const MasterMaterialHandle a_material)
{
    m_passes.emplace_back(a_arena, a_call, a_resources_in, a_resources_out, a_material);
    return m_passes[m_passes.size() - 1];
}

RG::ResourceHandle RG::RenderGraph::AddUniform(const StackString<32>& a_name, const size_t a_size, const void* a_upload_data)
{
    RG::RenderResource resource{};
    resource.name = a_name;
    resource.buffer.size = a_size;
    resource.upload_data = a_upload_data;
    resource.descriptor_type = DESCRIPTOR_TYPE::READONLY_CONSTANT;
    RG::ResourceHandle rh = RG::ResourceHandle(m_resources.size());
    m_resources.push_back(resource);
    return rh;
}

RG::ResourceHandle RG::RenderGraph::AddBuffer(const StackString<32>& a_name, const size_t a_size, const void* a_upload_data)
{
    RG::RenderResource resource{};
    resource.name = a_name;
    resource.buffer.size = a_size;
    resource.upload_data = a_upload_data;
    resource.descriptor_type = DESCRIPTOR_TYPE::READONLY_BUFFER;
    RG::ResourceHandle rh = RG::ResourceHandle(m_resources.size());
    m_resources.push_back(resource);
    return rh;
}

RG::ResourceHandle RG::RenderGraph::AddTexture(const StackString<32>& a_name, const uint3 a_extent, const uint16_t a_array_layers, const uint16_t a_mips, const IMAGE_USAGE a_usage, const IMAGE_FORMAT a_format, const void* a_upload_data)
{
    RG::RenderResource resource{};
    resource.name = a_name;
    resource.image.extent = a_extent;
    resource.image.array_layers = a_array_layers;
    resource.image.mips = a_mips;
    resource.image.format = a_format;
    resource.image.usage = a_usage;
    resource.upload_data = a_upload_data;
    resource.descriptor_type = DESCRIPTOR_TYPE::IMAGE;
    RG::ResourceHandle rh = RG::ResourceHandle(m_resources.size());
    m_resources.push_back(resource);
    return rh;
}

RG::ResourceHandle RG::RenderGraph::AddSampler(const StackString<32>& a_name, const RSampler a_sampler)
{
    RG::RenderResource resource{};
    resource.name = a_name;
    resource.sampler = a_sampler;
    resource.descriptor_type = DESCRIPTOR_TYPE::SAMPLER;
    RG::ResourceHandle rh = RG::ResourceHandle(m_resources.size());
    m_resources.push_back(resource);
    return rh;
}

const RG::RenderResource& RG::RenderGraph::GetResource(const RG::ResourceHandle a_handle)
{
    return m_resources[a_handle.handle];
}

void RG::RenderGraphSystem::Init(MemoryArena& a_arena, const uint32_t a_back_buffers, const uint32_t a_max_passes, const uint32_t a_max_resources)
{
    m_fence = CreateFence(0, "rendergraph fence");
    m_next_fence_value = 1;
    m_last_completed_fence_value = 0;

    m_upload_allocator.Init(a_arena, mbSize * 64, m_fence, "rendergraph upload allocator");
    m_graphs.Init(a_arena, a_back_buffers);
    for (uint32_t i = 0; i < a_back_buffers; i++)
    {
        m_graphs.emplace_back(a_arena, a_max_passes, a_max_resources);
    }
}

bool RG::RenderGraphSystem::StartGraph(const uint32_t a_back_buffer, RG::RenderGraph* a_out_graph)
{
    if (!m_graphs[a_back_buffer].IsFinished(m_last_completed_fence_value))
        return false;
    m_graphs[a_back_buffer].Reset();
    a_out_graph = &m_graphs[a_back_buffer];

    m_global.scene_info.per_frame_index = a_out_graph->GetPerFrameBufferDescriptorIndex();
    return true;
}

bool RG::RenderGraphSystem::ExecuteGraph(RenderGraph& a_graph)
{

    return true;
}

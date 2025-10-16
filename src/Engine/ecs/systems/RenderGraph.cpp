#include "Rendergraph.hpp"
#include "Renderer.hpp"

using namespace BB;

constexpr uint64_t STANDARD_COMPUTE_SIZE = 16 * 1028 * 1028;

RG::RenderPass::RenderPass(MemoryArena& a_arena, const PFN_RenderPass a_call, const uint32_t a_resources_in, const uint32_t a_resources_out, const MasterMaterialHandle a_material)
{
    m_call = a_call;
    m_material = a_material;
    if (a_resources_in)
        m_resource_inputs.Init(a_arena, a_resources_in);
    if (a_resources_out)
        m_resource_outputs.Init(a_arena, a_resources_out);
}

bool RG::RenderPass::DoPass(MemoryArena& a_temp_arena, class RenderGraph& a_graph, GlobalGraphData& a_global_data, const RCommandList a_list)
{
    return m_call(a_temp_arena, a_graph, a_global_data, a_list, m_material, m_resource_inputs.slice(), m_resource_outputs.slice());
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

RG::RenderGraph::RenderGraph(MemoryArena& a_arena, const uint32_t a_max_passes, const uint32_t a_max_resources, const uint64_t a_compute_size)
{
    m_passes.Init(a_arena, a_max_passes);
    m_execution_order.Init(a_arena, a_max_passes);
    m_resources.Init(a_arena, a_max_resources);

    m_per_frame_descriptor = AllocateBufferDescriptor();
    m_scene_buffer.Init(BUFFER_TYPE::UNIFORM, sizeof(Scene3DInfo), "scene info buffer");
    m_scene_descriptor = AllocateUniformDescriptor();
    DescriptorWriteUniformBuffer(m_scene_descriptor, m_scene_buffer.GetView());
}

bool RG::RenderGraph::Reset()
{
    for (uint32_t i = 0; i < m_resources.size(); i++)
    {
        const RenderResource& res = m_resources[i];
        if (res.descriptor_type == DESCRIPTOR_TYPE::IMAGE && res.rendergraph_owned)
        {
            FreeImageView(res.descriptor_index);
            FreeImage(res.image.image);
        }
    }

    m_passes.clear();
    m_execution_order.clear();
    m_resources.clear();
    m_per_frame_buffer.Clear();
    m_drawlist.transforms.clear();
    m_drawlist.draw_entries.clear();

    // delete resources here
    return true;
}

bool RG::RenderGraph::Compile(MemoryArena& a_arena, GPUUploadRingAllocator& a_upload_buffer, const uint64_t a_fence_value)
{
    for (uint32_t i = 0; i < m_passes.size(); i++)
    {
        m_execution_order.push_back(i);
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

    RenderCopyBufferRegion* pf_regions = ArenaAllocArr(a_arena, RenderCopyBufferRegion, pf_upload_region_count);
    uint32_t pf_current_region = 0;
    RenderCopyBufferToImageInfo* image_copies = ArenaAllocArr(a_arena, RenderCopyBufferToImageInfo, image_uploads);
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
            GPUBufferView bufview;
            success = m_per_frame_buffer.Allocate(res.buffer.size, bufview);

            res.buffer.buffer = bufview.buffer;
            res.buffer.size = bufview.size;
            res.buffer.offset = bufview.offset;
            res.buffer.address = GetGPUBufferAddress(bufview.buffer);

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
            if (!res.descriptor_index.IsValid())
            {
                ImageCreateInfo image_info;
                image_info.name = res.name.c_str();
                image_info.width = res.image.extent.x;
                image_info.height = res.image.extent.y;
                image_info.depth = res.image.extent.z;
                image_info.array_layers = res.image.array_layers;
                image_info.mip_levels = res.image.mips;
                if (res.image.extent.z == 1)
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
                view_info.image = res.image.image;
                view_info.array_layers = res.image.array_layers;
                view_info.mip_levels = res.image.mips;
                view_info.base_array_layer = 0;
                view_info.base_mip_level = 0;
                view_info.format = res.image.format;

                if (res.image.extent.z == 1)
                    if (res.image.array_layers == 1)
                        view_info.type = IMAGE_VIEW_TYPE::TYPE_2D;
                    else
                        view_info.type = IMAGE_VIEW_TYPE::TYPE_2D_ARRAY;
                else
                    view_info.type = IMAGE_VIEW_TYPE::TYPE_3D;

                if (res.image.usage == IMAGE_USAGE::DEPTH)
                    view_info.aspects = IMAGE_ASPECT::DEPTH_STENCIL;
                else if (res.image.usage == IMAGE_USAGE::SHADOW_MAP)
                    view_info.aspects = IMAGE_ASPECT::DEPTH;
                else
                    view_info.aspects = IMAGE_ASPECT::COLOR;

                res.descriptor_index = CreateImageView(view_info);
            }

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

                if (res.image.usage == IMAGE_USAGE::DEPTH)
                    image_copies[current_image_copy].dst_aspects = IMAGE_ASPECT::DEPTH_STENCIL;
                else if (res.image.usage == IMAGE_USAGE::SHADOW_MAP)
                    image_copies[current_image_copy].dst_aspects = IMAGE_ASPECT::DEPTH;
                else
                    image_copies[current_image_copy].dst_aspects = IMAGE_ASPECT::COLOR;

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
            // already allocated and handled
        }
    }

    m_per_frame_copies = Slice(pf_regions, pf_upload_region_count);
    m_image_copies = Slice(image_copies, image_uploads);

    return true;
}

CommandPool* RG::RenderGraph::Execute(MemoryArena& a_temp_arena, GlobalGraphData& a_global, const GPUBuffer a_upload_buffer)
{
    CommandPool& pool = GetGraphicsCommandPool();
    RCommandList list = pool.StartCommandList();

    BindGraphicsBindlessSet(list);
    BindIndexBuffer(list, 0);
    SetPushConstantsSceneUniformIndex(list, m_scene_descriptor);

    if (m_per_frame_copies.size())
    {
        RenderCopyBuffer copy_buffer;
        copy_buffer.src = a_upload_buffer;
        copy_buffer.dst = m_per_frame_buffer.GetBuffer();
        copy_buffer.regions = m_per_frame_copies;
        CopyBuffer(list, copy_buffer);
    }

    for (size_t i = 0; i < m_image_copies.size(); i++)
        CopyBufferToImage(list, m_image_copies[i]);

    for (uint32_t i = 0; i < m_execution_order.size(); i++)
    {
        MemoryArenaScope(a_temp_arena)
        {
            RenderPass& pass = m_passes[m_execution_order[i]];
            HandleImagetransitions(a_temp_arena, list, pass);
            if (!pass.DoPass(a_temp_arena, *this, a_global, list))
                return nullptr;
        }
    }

    m_scene_buffer.WriteTo(&a_global.scene_info, sizeof(a_global.scene_info), 0);
    pool.EndCommandList(list);
    return &pool;
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
    resource.rendergraph_owned = true;
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
    resource.rendergraph_owned = true;
    RG::ResourceHandle rh = RG::ResourceHandle(m_resources.size());
    m_resources.push_back(resource);
    return rh;
}

RG::ResourceHandle RG::RenderGraph::AddImage(const StackString<32>& a_name, const uint3 a_extent, const uint16_t a_array_layers, const uint16_t a_mips, const IMAGE_USAGE a_usage, const IMAGE_FORMAT a_format, const bool a_is_cube_map, const void* a_upload_data)
{
    RG::RenderResource resource{};
    resource.name = a_name;
    resource.image.extent = a_extent;
    resource.image.array_layers = a_array_layers;
    resource.image.mips = a_mips;
    resource.image.format = a_format;
    resource.image.usage = a_usage;
    resource.image.current_layout = IMAGE_LAYOUT::NONE;
    resource.image.is_cube_map = a_is_cube_map;
    resource.upload_data = a_upload_data;
    resource.rendergraph_owned = true; 
    resource.descriptor_type = DESCRIPTOR_TYPE::IMAGE;
    RG::ResourceHandle rh = RG::ResourceHandle(m_resources.size());
    m_resources.push_back(resource);
    return rh;
}

RG::ResourceHandle RG::RenderGraph::AddImage(const StackString<32>& a_name, const RImage a_image, const RDescriptorIndex a_index, const uint3 a_extent, const uint16_t a_array_layers, const uint16_t a_mips, const IMAGE_FORMAT a_format, const IMAGE_USAGE a_usage, const bool a_is_cube_map)
{
    RG::RenderResource resource{};
    resource.name = a_name;
    resource.descriptor_index = a_index;
    resource.image.image = a_image;
    resource.image.extent = a_extent;
    resource.image.array_layers = a_array_layers;
    resource.image.mips = a_mips;
    resource.image.format = a_format;
    resource.image.usage = a_usage;
    resource.image.current_layout = IMAGE_LAYOUT::RW_FRAGMENT;
    resource.image.is_cube_map = a_is_cube_map;
    resource.upload_data = nullptr;
    resource.descriptor_type = DESCRIPTOR_TYPE::IMAGE;
    resource.rendergraph_owned = false;
    RG::ResourceHandle rh = RG::ResourceHandle(m_resources.size());
    m_resources.push_back(resource);
    return rh;
}

RG::ResourceHandle RG::RenderGraph::AddSampler(const StackString<32>& a_name, const RDescriptorIndex a_sampler)
{
    RG::RenderResource resource{};
    resource.name = a_name;
    resource.descriptor_index = a_sampler;
    resource.descriptor_type = DESCRIPTOR_TYPE::SAMPLER;
    resource.rendergraph_owned = false;
    RG::ResourceHandle rh = RG::ResourceHandle(m_resources.size());
    m_resources.push_back(resource);
    return rh;
}

const RG::RenderResource& RG::RenderGraph::GetResource(const RG::ResourceHandle a_handle)
{
    return m_resources[a_handle.handle];
}

void RG::RenderGraph::HandleImagetransitions(MemoryArena& a_temp_arena, const RCommandList a_list, const RenderPass& a_pass)
{
    const ConstSlice in = a_pass.GetInputs();
    const ConstSlice out = a_pass.GetOutputs();
    const uint32_t resource_count = static_cast<uint32_t>(in.size() + out.size());
    if (resource_count == 0)
        return;

    auto TransitionImages = [](RenderGraph& a_graph, const ConstSlice<ResourceHandle> a_resources, StaticArray<PipelineBarrierImageInfo>& a_barriers, const IMAGE_LAYOUT a_wanted_layout, const IMAGE_LAYOUT a_wanted_depth)
        {
            for (size_t i = 0; i < a_resources.size(); i++)
            {
                RenderResource& res = a_graph.m_resources[a_resources[i].handle];
                if (res.descriptor_type == DESCRIPTOR_TYPE::IMAGE)
                {
                    const bool is_depth = res.image.usage == IMAGE_USAGE::DEPTH || res.image.usage == IMAGE_USAGE::SHADOW_MAP;
                    const IMAGE_LAYOUT wanted = is_depth ? a_wanted_depth : a_wanted_layout;
                    if (res.image.current_layout != wanted)
                    {
                        const uint32_t index = a_barriers.size();
                        a_barriers.emplace_back();
                        a_barriers[index].image = res.image.image;
                        a_barriers[index].prev = res.image.current_layout;
                        a_barriers[index].next = wanted;
                        a_barriers[index].layer_count = res.image.array_layers;
                        a_barriers[index].level_count = res.image.mips;
                        a_barriers[index].base_array_layer = res.image.base_array_layer;
                        a_barriers[index].base_mip_level = res.image.base_mip;
                        if (is_depth)
                        {
                            if (res.image.usage == IMAGE_USAGE::DEPTH)
                                a_barriers[index].image_aspect = IMAGE_ASPECT::DEPTH_STENCIL;
                            else
                                a_barriers[index].image_aspect = IMAGE_ASPECT::DEPTH;
                        }
                        else
                            a_barriers[index].image_aspect = IMAGE_ASPECT::COLOR;
                        res.image.current_layout = wanted;
                    }
                }
            }
        };

    MemoryArenaScope(a_temp_arena)
    {
        StaticArray<PipelineBarrierImageInfo> im_ba{};
        im_ba.Init(a_temp_arena, resource_count);
        TransitionImages(*this, in, im_ba, IMAGE_LAYOUT::RW_FRAGMENT, IMAGE_LAYOUT::RW_FRAGMENT);
        TransitionImages(*this, out, im_ba, IMAGE_LAYOUT::RT_COLOR, IMAGE_LAYOUT::RT_DEPTH);

        if (im_ba.size())
        {
            PipelineBarrierInfo barriers{};
            barriers.image_barriers = im_ba.const_slice();
            PipelineBarriers(a_list, barriers);
        }
    }
}

void RG::RenderGraph::AddDrawEntry(const DrawList::DrawEntry& a_draw_entry, const ShaderTransform& a_transform)
{
    m_drawlist.draw_entries.push_back(a_draw_entry);
    m_drawlist.transforms.push_back(a_transform);
}

void RG::RenderGraph::SetupDrawList(MemoryArena& a_arena, const uint32_t a_size)
{
    m_drawlist.draw_entries.Init(a_arena, a_size);
    m_drawlist.transforms.Init(a_arena, a_size);
}

void RG::RenderGraphSystem::Init(MemoryArena& a_arena, const uint32_t a_back_buffers, const uint32_t a_max_passes, const uint32_t a_max_resources)
{
    m_fence = CreateFence(0, "rendergraph fence");
    m_next_fence_value = 1;
    m_last_completed_fence_value = 0;

    m_upload_allocator.Init(a_arena, mbSize * 64, m_fence, "rendergraph upload allocator");
    m_graphs.Init(a_arena, a_back_buffers);
    for (uint32_t i = 0; i < a_back_buffers; i++)
        m_graphs.emplace_back(a_arena, a_max_passes, a_max_resources, STANDARD_COMPUTE_SIZE);
}

bool RG::RenderGraphSystem::StartGraph(MemoryArena& a_arena, const uint32_t a_back_buffer, RG::RenderGraph*& a_out_graph, const uint32_t a_draw_list_size)
{
    if (!m_graphs[a_back_buffer].IsFinished(m_last_completed_fence_value))
        return false;
    m_graphs[a_back_buffer].Reset();
    a_out_graph = &m_graphs[a_back_buffer];

    m_global.scene_info.per_frame_index = a_out_graph->GetPerFrameBufferDescriptorIndex();
    if (a_draw_list_size)
        a_out_graph->SetupDrawList(a_arena, a_draw_list_size);

    return true;
}

bool RG::RenderGraphSystem::CompileGraph(MemoryArena& a_arena, RG::RenderGraph& a_graph)
{
    return a_graph.Compile(a_arena, m_upload_allocator, m_next_fence_value);
}

CommandPool* RG::RenderGraphSystem::ExecuteGraph(MemoryArena& a_temp_arena, RenderGraph& a_graph)
{
    return a_graph.Execute(a_temp_arena, m_global, m_upload_allocator.GetBuffer());
}

bool RG::RenderGraphSystem::EndGraph(RenderGraph& a_graph)
{
    (void)a_graph;
    return true;
}

const GPUBufferView RG::RenderGraphSystem::AllocateAndUploadGPUMemory(const size_t a_data_size, const void* a_data)
{
    GPUBufferView view;
    view.buffer = m_upload_allocator.GetBuffer();
    view.size = a_data_size;
    view.offset = m_upload_allocator.AllocateUploadMemory(a_data_size, m_next_fence_value);
    
    const bool success = m_upload_allocator.MemcpyIntoBuffer(view.offset, a_data, a_data_size);
    BB_ASSERT(success, "failed to upload GPU memory");
    return view;
}

void RG::RenderGraphSystem::WaitFence() const
{
    ::WaitFence(m_fence, m_next_fence_value - 1);
}

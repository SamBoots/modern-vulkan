#include "Rendergraph.hpp"

using namespace BB;

RG::RenderPass::RenderPass(MemoryArena& a_arena, const PFN_RenderPass a_call, const uint32_t a_resources_in, const uint32_t a_resources_out, const MasterMaterialHandle a_material)
{
    m_call = a_call;
    m_material = a_material;
    m_resource_inputs.Init(a_arena, a_resources_in);
    m_resource_outputs.Init(a_arena, a_resources_out);
}

bool RG::RenderPass::DoPass(class RenderGraph& a_graph, const RCommandList a_list)
{
    return m_call(a_graph, a_list, m_material, m_resource_inputs.slice(), m_resource_outputs.slice());
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

RG::ResourceHandle RG::RenderGraph::AddTexture(const StackString<32>& a_name, const uint3 a_extent, const IMAGE_FORMAT a_format, const void* a_upload_data)
{
    RG::RenderResource resource{};
    resource.name = a_name;
    resource.image.extent = a_extent;
    resource.image.format = a_format;
    resource.upload_data = a_upload_data;
    resource.descriptor_type = DESCRIPTOR_TYPE::IMAGE;
    RG::ResourceHandle rh = RG::ResourceHandle(m_resources.size());
    m_resources.push_back(resource);
    return rh;
}

RG::ResourceHandle RG::RenderGraph::AddRenderTarget(const StackString<32>& a_name, const uint3 a_extent, const IMAGE_FORMAT a_format)
{
    RG::RenderResource resource{};
    resource.name = a_name;
    resource.image.extent = a_extent;
    resource.image.format = a_format;
    resource.upload_data = nullptr;
    resource.descriptor_type = DESCRIPTOR_TYPE::IMAGE;
    RG::ResourceHandle rh = RG::ResourceHandle(m_resources.size());
    m_resources.push_back(resource);
    return rh;
}

RG::ResourceHandle RG::RenderGraph::AddDepthTarget(const StackString<32>& a_name, const uint3 a_extent, const IMAGE_FORMAT a_format)
{
    RG::RenderResource resource{};
    resource.name = a_name;
    resource.image.extent = a_extent;
    resource.image.format = a_format;
    resource.upload_data = nullptr;
    resource.descriptor_type = DESCRIPTOR_TYPE::IMAGE;
    RG::ResourceHandle rh = RG::ResourceHandle(m_resources.size());
    m_resources.push_back(resource);
    return rh;
}

const RG::RenderResource& RG::RenderGraph::GetResource(const RG::ResourceHandle a_handle)
{
    return m_resources[a_handle.handle];
}

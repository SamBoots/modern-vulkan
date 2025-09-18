#pragma once
#include "GPUBuffers.hpp"
#include "Rendererfwd.hpp"
#include "ecs/components/RenderComponent.hpp"
#include "ecs/components/RaytraceComponent.hpp"

#include "ClearStage.hpp"
#include "ShadowMapStage.hpp"
#include "RasterMeshStage.hpp"
#include "BloomStage.hpp"
#include "LineStage.hpp"
#include "UICanvas.hpp"

namespace BB
{   
    namespace RG
    {
        using ResourceHandle = FrameworkHandle32Bit<struct ResourceHandleTag>;


        struct RenderResource
        {
            StackString<32> name;
            RDescriptorIndex descriptor_index;
            DESCRIPTOR_TYPE descriptor_type;
            const void* upload_data;  // nullptr no upload
            struct ImageView
            {
                RImage image;
                IMAGE_FORMAT format;
                uint3 extent;
                uint32_t mip_level;
                uint16_t base_array_layer;
                uint16_t layer_count;
            };
            union
            {
                GPUBufferView buffer;
                ImageView image;
                RSampler sampler;
            };
        };

        typedef bool (*PFN_RenderPass)(class RenderGraph& a_graph, const RCommandList a_list, const MasterMaterialHandle a_material, Slice<ResourceHandle> a_resource_inputs, Slice<ResourceHandle> a_resource_outputs);

        class RenderPass
        {
        public:
            RenderPass(MemoryArena& a_arena, const PFN_RenderPass a_call, const uint32_t a_resources_in, const uint32_t a_resources_out, const MasterMaterialHandle a_material);

            bool DoPass(class RenderGraph& a_graph, const RCommandList a_list);

            bool AddInputResource(const ResourceHandle a_resource_index);
            bool AddOutputResource(const ResourceHandle a_resource_index);

        private:
            PFN_RenderPass m_call;
            MasterMaterialHandle m_material;

            StaticArray<ResourceHandle> m_resource_inputs;
            StaticArray<ResourceHandle> m_resource_outputs;
        };

        class RenderGraph
        {
        public:
            RenderGraph(MemoryArena& a_arena, const uint32_t a_max_passes, const uint32_t a_max_resources);

            RenderPass& AddRenderPass(MemoryArena& a_arena, const PFN_RenderPass a_call, const uint32_t a_resources_in, const uint32_t a_resources_out, const MasterMaterialHandle a_material);
            ResourceHandle AddUniform(const StackString<32>& a_name, const size_t a_size, const void* a_upload_data = nullptr);
            ResourceHandle AddBuffer(const StackString<32>& a_name, const size_t a_size, const void* a_upload_data = nullptr);
            ResourceHandle AddTexture(const StackString<32>& a_name, const uint3 a_extent, const IMAGE_FORMAT a_format, const void* a_upload_data = nullptr);
            ResourceHandle AddRenderTarget(const StackString<32>& a_name, const uint3 a_extent, const IMAGE_FORMAT a_format);
            ResourceHandle AddDepthTarget(const StackString<32>& a_name, const uint3 a_extent, const IMAGE_FORMAT a_format);

            const RenderResource& GetResource(const ResourceHandle a_handle);

        private:
            StaticArray<RenderPass> m_passes;
            StaticArray<uint32_t> m_execution_order;
            StaticArray<RenderResource> m_resources;
        };
    }
}

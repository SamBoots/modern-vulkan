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

        struct GlobalGraphData
        {
            Scene3DInfo scene_info;
            struct PostFXOptions
            {
                float blur_strength;
                float blur_scale;
            } post_fx;
            DrawList drawlist;
        };

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
                IMAGE_USAGE usage;
                uint3 extent;
                uint16_t array_layers;
                uint16_t mips;
                uint16_t base_array_layer;
                uint16_t base_mip;
            };
            union
            {
                GPUBufferView buffer;
                ImageView image;
                RSampler sampler;
            };
        };

        typedef bool (*PFN_RenderPass)(class RenderGraph& a_graph, GlobalGraphData& a_global_data, const RCommandList a_list, const MasterMaterialHandle a_material, Slice<ResourceHandle> a_resource_inputs, Slice<ResourceHandle> a_resource_outputs);

        class RenderPass
        {
        public:
            RenderPass(MemoryArena& a_arena, const PFN_RenderPass a_call, const uint32_t a_resources_in, const uint32_t a_resources_out, const MasterMaterialHandle a_material);

            bool DoPass(class RenderGraph& a_graph, GlobalGraphData& a_global_data, const RCommandList a_list);

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

            bool Reset();
            bool Compile(MemoryArena& a_temp_arena, GPUUploadRingAllocator& a_upload_buffer, const uint64_t a_fence_value);

            RenderPass& AddRenderPass(MemoryArena& a_arena, const PFN_RenderPass a_call, const uint32_t a_resources_in, const uint32_t a_resources_out, const MasterMaterialHandle a_material);
            ResourceHandle AddUniform(const StackString<32>& a_name, const size_t a_size, const void* a_upload_data = nullptr);
            ResourceHandle AddBuffer(const StackString<32>& a_name, const size_t a_size, const void* a_upload_data = nullptr);
            ResourceHandle AddTexture(const StackString<32>& a_name, const uint3 a_extent, const uint16_t a_array_layers, const uint16_t a_mips, const IMAGE_USAGE a_usage, const IMAGE_FORMAT a_format, const void* a_upload_data = nullptr);
            ResourceHandle AddSampler(const StackString<32>& a_name, const RSampler a_sampler);

            const RenderResource& GetResource(const ResourceHandle a_handle);
            const DrawList& GetDrawList() const { return m_drawlist; }
            const RDescriptorIndex GetPerFrameBufferDescriptorIndex() const { return m_per_frame_descriptor; }

            bool IsFinished(const uint64_t a_completed_fence_value) const { return a_completed_fence_value >= m_fence_value; }

        private:
            StaticArray<RenderPass> m_passes;
            StaticArray<uint32_t> m_execution_order;
            StaticArray<RenderResource> m_resources;

            DrawList m_drawlist;

            // scene data
            RDescriptorIndex m_per_frame_descriptor;
            GPULinearBuffer m_per_frame_buffer;
            GPUStaticCPUWriteableBuffer m_scene_buffer;
            uint64_t m_fence_value;
        };

        class RenderGraphSystem
        {
        public:
            void Init(MemoryArena& a_arena, const uint32_t a_back_buffers, const uint32_t a_max_passes, const uint32_t a_max_resources);
            bool StartGraph(const uint32_t a_back_buffer, RG::RenderGraph* a_out_graph);
            bool ExecuteGraph(RenderGraph& a_graph);

            GlobalGraphData& GetGlobalData() { return m_global; }

        private:
            StaticArray<RenderGraph> m_graphs;

            GlobalGraphData m_global;

            RFence m_fence;
            uint64_t m_next_fence_value;
            uint64_t m_last_completed_fence_value;
            GPUUploadRingAllocator m_upload_allocator;
        };
    }
}

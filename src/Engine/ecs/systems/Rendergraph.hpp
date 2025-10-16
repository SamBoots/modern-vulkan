#pragma once
#include "GPUBuffers.hpp"
#include "Rendererfwd.hpp"
#include "Enginefwd.hpp"

namespace BB
{   
    class CommandPool;
    struct LineColor
    {
        constexpr LineColor() : r(0), g(0), b(0), ignore_depth(false) {}
        constexpr explicit LineColor(const uint8_t a_r, const uint8_t a_g, const uint8_t a_b, const bool a_ignore_depth) : r(a_r), g(a_g), b(a_b), ignore_depth(a_ignore_depth) {}

        uint8_t r;
        uint8_t g;
        uint8_t b;
        uint8_t ignore_depth;
    };

    struct Line
    {
        float3 p0;
        LineColor p0_color;
        float3 p1;
        LineColor p1_color;
    };

    struct DrawList
    {
        struct DrawEntry
        {
            Mesh mesh;
            MasterMaterialHandle master_material;
            MaterialHandle material;
            uint32_t index_start;
            uint32_t index_count;
        };

        StaticArray<DrawEntry> draw_entries;
        StaticArray<ShaderTransform> transforms;
    };

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
            struct Raytracing
            {
                ConstSlice<ConstSlice<AccelerationStructGeometrySize>> blas_geo_sizes;
                ConstSlice<ConstSlice<uint32_t>> blas_primitive_counts;
                ConstSlice<BottomLevelAccelerationStructInstance> tlas_blas_instances;
                ConstSlice<uint32_t> tlas_primitive_count;
            } raytracing;
        };

        struct RenderResource
        {
            StackString<32> name;
            RDescriptorIndex descriptor_index;
            DESCRIPTOR_TYPE descriptor_type;
            const void* upload_data;  // nullptr no upload
            bool rendergraph_owned; // temp, due to the rendergraph deleting images
            struct ImageView
            {
                RImage image;
                IMAGE_FORMAT format;
                IMAGE_USAGE usage;
                IMAGE_LAYOUT current_layout;
                uint3 extent;
                uint16_t array_layers;
                uint16_t mips;
                uint16_t base_array_layer;
                uint16_t base_mip;
                bool is_cube_map;
            };
            struct AccelStruct
            {
                RAccelerationStruct structure;
                GPUAddress structure_address;
                bool must_build;
            };
            struct BufferView
            {
                GPUBuffer buffer;
                uint64_t size;
                uint64_t offset;
                GPUAddress address;
            };
            
            union
            {
                BufferView buffer;
                ImageView image;
                AccelStruct acceleration_structure;
            };
        };

        typedef bool (*PFN_RenderPass)(MemoryArena& a_temp_arena, class RenderGraph& a_graph, GlobalGraphData& a_global_data, const RCommandList a_list, const MasterMaterialHandle a_material, Slice<ResourceHandle> a_resource_inputs, Slice<ResourceHandle> a_resource_outputs);

        class RenderPass
        {
        public:
            RenderPass(MemoryArena& a_arena, const PFN_RenderPass a_call, const uint32_t a_resources_in, const uint32_t a_resources_out, const MasterMaterialHandle a_material);

            bool DoPass(MemoryArena& a_temp_arena, class RenderGraph& a_graph, GlobalGraphData& a_global_data, const RCommandList a_list);

            bool AddInputResource(const ResourceHandle a_resource_index);
            bool AddOutputResource(const ResourceHandle a_resource_index);

            ConstSlice<ResourceHandle> GetInputs() const { return m_resource_inputs.const_slice(); }
            ConstSlice<ResourceHandle> GetOutputs() const { return m_resource_outputs.const_slice(); }

        private:
            PFN_RenderPass m_call;
            MasterMaterialHandle m_material;

            StaticArray<ResourceHandle> m_resource_inputs;
            StaticArray<ResourceHandle> m_resource_outputs;
        };

        class RenderGraph
        {
        public:
            RenderGraph(MemoryArena& a_arena, const uint32_t a_max_passes, const uint32_t a_max_resources, const uint64_t a_compute_size);

            bool Reset();
            bool Compile(MemoryArena& a_arena, GPUUploadRingAllocator& a_upload_buffer, const uint64_t a_fence_value);
            CommandPool* Execute(MemoryArena& a_temp_arena, GlobalGraphData& a_global, const GPUBuffer a_upload_buffer);

            RenderPass& AddRenderPass(MemoryArena& a_arena, const PFN_RenderPass a_call, const uint32_t a_resources_in, const uint32_t a_resources_out, const MasterMaterialHandle a_material);
            ResourceHandle AddUniform(const StackString<32>& a_name, const size_t a_size, const void* a_upload_data = nullptr);
            ResourceHandle AddBuffer(const StackString<32>& a_name, const size_t a_size, const void* a_upload_data = nullptr);

            ResourceHandle AddImage(const StackString<32>& a_name, const uint3 a_extent, const uint16_t a_array_layers, const uint16_t a_mips, const IMAGE_USAGE a_usage, const IMAGE_FORMAT a_format, const bool a_is_cube_map = false, const void* a_upload_data = nullptr);
            ResourceHandle AddImage(const StackString<32>& a_name, const RImage a_image, const RDescriptorIndex a_index, const uint3 a_extent, const uint16_t a_array_layers, const uint16_t a_mips, const IMAGE_FORMAT a_format, const IMAGE_USAGE a_usage, const bool a_is_cube_map = false);

            ResourceHandle AddSampler(const StackString<32>& a_name, const RDescriptorIndex a_sampler);

            const RenderResource& GetResource(const ResourceHandle a_handle);
            const DrawList& GetDrawList() const { return m_drawlist; }
            const RDescriptorIndex GetPerFrameBufferDescriptorIndex() const { return m_per_frame_descriptor; }

            bool IsFinished(const uint64_t a_completed_fence_value) const { return a_completed_fence_value >= m_fence_value; }

            void SetupDrawList(MemoryArena& a_arena, const uint32_t a_size);
            void AddDrawEntry(const DrawList::DrawEntry& a_draw_entry, const ShaderTransform& a_transform);

        private:
            void HandleImagetransitions(MemoryArena& a_temp_arena, const RCommandList a_list, const RenderPass& a_pass);

            StaticArray<RenderPass> m_passes;
            StaticArray<uint32_t> m_execution_order;
            StaticArray<RenderResource> m_resources;

            Slice<RenderCopyBufferRegion> m_per_frame_copies;
            Slice<RenderCopyBufferToImageInfo> m_image_copies;

            DrawList m_drawlist;

            // scene data
            RDescriptorIndex m_per_frame_descriptor;
            GPULinearBuffer m_per_frame_buffer;

            GPUStaticCPUWriteableBuffer m_scene_buffer;
            RDescriptorIndex m_scene_descriptor;
            uint64_t m_fence_value;
        };

        class RenderGraphSystem
        {
        public:
            void Init(MemoryArena& a_arena, const uint32_t a_back_buffers, const uint32_t a_max_passes, const uint32_t a_max_resources);
            bool StartGraph(MemoryArena& a_arena, const uint32_t a_back_buffer, RG::RenderGraph*& a_out_graph, const uint32_t a_draw_list_size);
            bool CompileGraph(MemoryArena& a_arena, RG::RenderGraph& a_graph);
            CommandPool* ExecuteGraph(MemoryArena& a_temp_arena, RenderGraph& a_graph);
            bool EndGraph(RenderGraph& a_graph);
            const GPUBufferView AllocateAndUploadGPUMemory(const size_t a_data_size, const void* a_data);

            void WaitFence() const;
            GlobalGraphData& GetGlobalData() { return m_global; }
            const GlobalGraphData& GetConstGlobalData() const { return m_global; }

            uint64_t NextFenceValue() const { return m_next_fence_value; }
            uint64_t IncrementNextFenceValue() { return m_next_fence_value++; }
            RFence GetFence() const { return m_fence; }
 
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

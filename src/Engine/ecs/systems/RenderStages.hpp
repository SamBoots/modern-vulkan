#pragma once
#include "Rendergraph.hpp"

namespace BB
{
    bool RenderPassClearStage(MemoryArena& a_temp_arena, RG::RenderGraph& a_graph, RG::GlobalGraphData& a_global_data, const RCommandList a_list, const MasterMaterialHandle a_material, Slice<RG::ResourceHandle> a_resource_inputs, Slice<RG::ResourceHandle> a_resource_outputs);
    bool RenderPassShadowMapStage(MemoryArena& a_temp_arena, RG::RenderGraph& a_graph, RG::GlobalGraphData& a_global_data, const RCommandList a_list, const MasterMaterialHandle a_material, Slice<RG::ResourceHandle> a_resource_inputs, Slice<RG::ResourceHandle> a_resource_outputs);
    bool RenderPassPBRStage(MemoryArena& a_temp_arena, RG::RenderGraph& a_graph, RG::GlobalGraphData& a_global_data, const RCommandList a_list, const MasterMaterialHandle a_material, Slice<RG::ResourceHandle> a_resource_inputs, Slice<RG::ResourceHandle> a_resource_outputs);
    bool RenderPassLineStage(MemoryArena& a_temp_arena, RG::RenderGraph& a_graph, RG::GlobalGraphData& a_global_data, const RCommandList a_list, const MasterMaterialHandle a_material, Slice<RG::ResourceHandle> a_resource_inputs, Slice<RG::ResourceHandle> a_resource_outputs);
    bool RenderPassGlyphStage(MemoryArena& a_temp_arena, RG::RenderGraph& a_graph, RG::GlobalGraphData& a_global_data, const RCommandList a_list, const MasterMaterialHandle a_material, Slice<RG::ResourceHandle> a_resource_inputs, Slice<RG::ResourceHandle> a_resource_outputs);
    bool RenderPassBloomStage(MemoryArena& a_temp_arena, RG::RenderGraph& a_graph, RG::GlobalGraphData& a_global_data, const RCommandList a_list, const MasterMaterialHandle a_material, Slice<RG::ResourceHandle> a_resource_inputs, Slice<RG::ResourceHandle> a_resource_outputs);

    bool RenderPassBLASes(MemoryArena& a_temp_arena, RG::RenderGraph& a_graph, RG::GlobalGraphData& a_global_data, const RCommandList a_list, const MasterMaterialHandle a_material, Slice<RG::ResourceHandle> a_resource_inputs, Slice<RG::ResourceHandle> a_resource_outputs);
    bool RenderPassTLAS(MemoryArena& a_temp_arena, RG::RenderGraph& a_graph, RG::GlobalGraphData& a_global_data, const RCommandList a_list, const MasterMaterialHandle a_material, Slice<RG::ResourceHandle> a_resource_inputs, Slice<RG::ResourceHandle> a_resource_outputs);

}

#pragma once
#include "Rendergraph.hpp"

namespace BB
{
    bool RenderPassClearStage(RG::RenderGraph& a_graph, const RCommandList a_list, const MasterMaterialHandle a_material, Slice<RG::ResourceHandle> a_resource_inputs, Slice<RG::ResourceHandle> a_resource_outputs);
    bool RenderPassShadowMapStage(RG::RenderGraph& a_graph, const RCommandList a_list, const MasterMaterialHandle a_material, Slice<RG::ResourceHandle> a_resource_inputs, Slice<RG::ResourceHandle> a_resource_outputs);
    bool RenderPassPBRStage(RG::RenderGraph& a_graph, const RCommandList a_list, const MasterMaterialHandle a_material, Slice<RG::ResourceHandle> a_resource_inputs, Slice<RG::ResourceHandle> a_resource_outputs);
    bool RenderPassLineStage(RG::RenderGraph& a_graph, const RCommandList a_list, const MasterMaterialHandle a_material, Slice<RG::ResourceHandle> a_resource_inputs, Slice<RG::ResourceHandle> a_resource_outputs);
    bool RenderPassGlyphStage(RG::RenderGraph& a_graph, const RCommandList a_list, const MasterMaterialHandle a_material, Slice<RG::ResourceHandle> a_resource_inputs, Slice<RG::ResourceHandle> a_resource_outputs);
    bool RenderPassBloomStage(RG::RenderGraph& a_graph, const RCommandList a_list, const MasterMaterialHandle a_material, Slice<RG::ResourceHandle> a_resource_inputs, Slice<RG::ResourceHandle> a_resource_outputs);
}

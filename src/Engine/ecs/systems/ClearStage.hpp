#pragma once
#include "GPUBuffers.hpp"
#include "Rendererfwd.hpp"
#include "Enginefwd.hpp"

namespace BB
{
    class ClearStage
    {
    public:
        void Init(MemoryArena& a_arena);
        void ExecutePass(const RCommandList a_list, const uint2 a_draw_area_size, const RImageView a_render_target);
        void UpdateConstantBuffer(Scene3DInfo& a_scene_3d_info) const;
    private:
        RDescriptorIndex m_skybox_descriptor_index;
        RImage m_skybox;

        MasterMaterialHandle m_skybox_material;
    };
}

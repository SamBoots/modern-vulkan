#pragma once
#include "GPUBuffers.hpp"
#include "Rendererfwd.hpp"
#include "Enginefwd.hpp"

namespace BB
{
    class BloomStage
    {
    public:
        void Init(MemoryArena& a_arena);
        void ExecutePass(const RCommandList a_list, const uint2 a_resolution, const RImage a_render_target_image, const RDescriptorIndex a_render_target_0, const RDescriptorIndex a_render_target_1, const uint2 a_draw_area, const RImageView a_render_target);
        //void UpdateConstantBuffer(Scene3DInfo& a_scene_3d_info) const;
    private:
        float m_bloom_strength;
        float m_bloom_scale;

        MasterMaterialHandle m_gaussian_material;
    };
}

#pragma once
#include "Common.h"

namespace BB
{
    struct RenderOptions
    {
        uint2 resolution;
        uint2 shadow_map_resolution;
        uint32_t bloom_downscale_factor;
        uint32_t bloom_smoothness;
        IMAGE_FORMAT backbuffer_format;
        bool triple_buffering;
    };

    namespace RENDER_OPTIONS
    {
        constexpr uint2 RESOLUTIONS[]
        {
            uint2(800, 600),
            uint2(1280, 720),
            uint2(1366, 768),
            uint2(1920, 1080),
            uint2(2560, 1440),
            uint2(3840, 2160)
        };

        constexpr uint2 SHADOW_MAP_RESOLUTION[]
        {
            uint2(512),
            uint2(1024),
            uint2(2048),
            uint2(4096),
            uint2(8192)
        };

        constexpr uint32_t BLOOM_DOWNSCALE_FACTOR[]
        {
            1,
            2,
            4,
            8
        };
        
        constexpr uint32_t BLOOM_SMOOTHNESS[]
        {
            1,
            2,
            3,
            4
        };
    }
}

#pragma once
#include "Common.h"

namespace BB
{
    static inline bool BoxRayIntersect(const float3& a_box_min, const float3& a_box_max, const float3& a_ray_origin, const float3& a_ray_dir)
    {
        float tmin = (a_box_min.x - a_ray_origin.x) / a_ray_dir.x;
        float tmax = (a_box_max.x - a_ray_origin.x) / a_ray_dir.x;

        if (tmin > tmax) std::swap(tmin, tmax);

        float tymin = (a_box_min.y - a_ray_origin.y) / a_ray_dir.y;
        float tymax = (a_box_max.y - a_ray_origin.y) / a_ray_dir.y;

        if (tymin > tymax) std::swap(tymin, tymax);

        if ((tmin > tymax) || (tymin > tmax))
            return false;

        if (tymin > tmin) tmin = tymin;
        if (tymax < tmax) tmax = tymax;

        float tzmin = (a_box_min.z - a_ray_origin.z) / a_ray_dir.z;
        float tzmax = (a_box_max.z - a_ray_origin.z) / a_ray_dir.z;

        if (tzmin > tzmax) std::swap(tzmin, tzmax);

        if ((tmin > tzmax) || (tzmin > tmax))
            return false;

        if (tzmin > tmin) tmin = tzmin;
        if (tzmax < tmax) tmax = tzmax;

        return true;
    }
    
    static inline void ScaleBoundingBox(const float3& a_min, const float3& a_max, const float3& a_scale, float3& a_new_min, float3& a_new_max)
    {
        const float3 center = (a_min + a_max) / 2.f;
        const float3 half_size = (a_max - a_min) / 2.f;
        const float3 new_half_size = half_size * a_scale;

        a_new_min = center - new_half_size;
        a_new_max = center + new_half_size;
    }
}

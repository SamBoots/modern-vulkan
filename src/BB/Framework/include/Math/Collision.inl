#pragma once
#include "Common.h"

namespace BB
{
    static inline bool _BoxRayIntersect(const float3& a_box_min, const float3& a_box_max, const float3& a_ray_origin, const float3& a_ray_dir, float& a_tmin, float& a_tmax)
    {
        a_tmin = (a_box_min.x - a_ray_origin.x) / a_ray_dir.x;
        a_tmax = (a_box_max.x - a_ray_origin.x) / a_ray_dir.x;

        if (a_tmin > a_tmax) std::swap(a_tmin, a_tmax);

        float tymin = (a_box_min.y - a_ray_origin.y) / a_ray_dir.y;
        float tymax = (a_box_max.y - a_ray_origin.y) / a_ray_dir.y;

        if (tymin > tymax) std::swap(tymin, tymax);

        if ((a_tmin > tymax) || (tymin > a_tmax))
            return false;

        if (tymin > a_tmin) a_tmin = tymin;
        if (tymax < a_tmax) a_tmax = tymax;

        float tzmin = (a_box_min.z - a_ray_origin.z) / a_ray_dir.z;
        float tzmax = (a_box_max.z - a_ray_origin.z) / a_ray_dir.z;

        if (tzmin > tzmax) std::swap(tzmin, tzmax);

        if ((a_tmin > tzmax) || (tzmin > a_tmax))
            return false;

        if (tzmin > a_tmin) a_tmin = tzmin;
        if (tzmax < a_tmax) a_tmax = tzmax;

        if (a_tmax < 0)
            return false;

        return true;
    }

    static inline bool BoxRayIntersect(const float3& a_box_min, const float3& a_box_max, const float3& a_ray_origin, const float3& a_ray_dir)
    {
        float tmin;
        float tmax;
        return _BoxRayIntersect(a_box_min, a_box_max, a_ray_origin, a_ray_dir, tmin, tmax);
    }

    static inline bool BoxRayIntersectLength(const float3& a_box_min, const float3& a_box_max, const float3& a_ray_origin, const float3& a_ray_dir, float& a_length)
    {
        float tmin;
        float tmax;
        if (_BoxRayIntersect(a_box_min, a_box_max, a_ray_origin, a_ray_dir, tmin, tmax))
        {
            a_length = tmin;
            return true;
        }
        else
            return false;
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

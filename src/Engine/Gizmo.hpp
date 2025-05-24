#pragma once
#include "Enginefwd.hpp"

namespace BB
{
    enum class GIZMO_MODE : uint32_t
    {
        TRANSLATE,
        SCALE
    };

    using GIZMO_HIT_FLAGS = uint32_t;
    enum class GIZMO_HIT : GIZMO_HIT_FLAGS
    {
        RIGHT = 1 << 1,
        UP = 1 << 2,
        FORWARD = 1 << 3,
    };

    struct Gizmo
    {
        float3 pos;
        float arrow_length;
    };

    Gizmo CreateGizmo(class EntityComponentSystem& a_ecs, const ECSEntity a_entity);
    GIZMO_HIT_FLAGS GizmoCollide(const Gizmo& a_gizmo, const float3 a_ray_origin, const float3 a_ray_dir);
    void DrawGizmo(class EntityComponentSystem& a_ecs, const Gizmo& a_gizmo, const GIZMO_HIT_FLAGS a_hits);
    void GizmoManipulateEntity(class EntityComponentSystem& a_ecs, const ECSEntity a_entity, const GIZMO_HIT_FLAGS a_gizmo_hits, const float2 a_mouse_move);
}

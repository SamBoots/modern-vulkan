#pragma once
#include "Enginefwd.hpp"

namespace BB
{
    enum class GIZMO_MODE : uint32_t
    {
        TRANSLATE,
        SCALE,
        ROTATE
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
        ECSEntity selected_entity;
        GIZMO_MODE mode;
        GIZMO_HIT_FLAGS hit_flags;
    };

    Gizmo CreateGizmo(const ECSEntity a_entity, const GIZMO_MODE a_mode);

    bool GizmoCollide(class EntityComponentSystem& a_ecs, Gizmo& a_gizmo, const float3 a_ray_origin, const float3 a_ray_dir);
    void DrawGizmo(class EntityComponentSystem& a_ecs, const Gizmo& a_gizmo);
    void GizmoManipulateEntity(class EntityComponentSystem& a_ecs, const ECSEntity a_entity, const Gizmo& a_gizmo, const float2 a_mouse_move);
}

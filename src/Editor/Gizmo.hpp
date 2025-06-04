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

    class Gizmo
    {
    public:
        void SelectEntity(const ECSEntity a_entity, class EntityComponentSystem* a_ecs);
        void UnselectEntity();
        void SetMode(const GIZMO_MODE a_mode);

        bool RayCollide(const float3 a_ray_origin, const float3 a_ray_dir);
        void ClearCollision();

        void Draw() const;
        void ManipulateEntity(const float2 a_mouse_move);

        bool HasValidEntity() const;
    private:
        GIZMO_HIT_FLAGS _RayCollide(const float3 a_ray_origin, const float3 a_ray_dir) const;

        class EntityComponentSystem* m_ecs = nullptr;
        ECSEntity m_selected_entity = INVALID_ECS_OBJ;
        GIZMO_MODE m_mode = GIZMO_MODE::TRANSLATE;
        GIZMO_HIT_FLAGS m_hit_flags = 0;
    };
}

#include "Gizmo.hpp"
#include "ecs/EntityComponentSystem.hpp"
#include "Math/Math.inl"
#include "Math/Collision.inl"

using namespace BB;

static float GetQuadSize(const float a_scale)
{
    constexpr float scale_size_len_mod = 0.25f;
    return a_scale * scale_size_len_mod;
}

struct GizmoPosAndScale
{
    float3 pos;
    float scale;
};

static GizmoPosAndScale GetGizmoPosAndScale(EntityComponentSystem& a_ecs, const ECSEntity a_entity)
{
    const BoundingBox& box = a_ecs.GetBoundingBox(a_entity);
    const float4x4& world_mat = a_ecs.GetWorldMatrix(a_entity);
    const float3 scale = Float4x4ExtractScale(world_mat);
    const float3 mod_scale = scale * Float3Distance(box.min, box.max);

    GizmoPosAndScale posscale;
    posscale.pos = Float4x4ExtractTranslation(world_mat) + float3(0.f, mod_scale.y / 2, 0.f);
    posscale.scale = (mod_scale.x + mod_scale.y + mod_scale.z) / 3;
    return posscale;
}

void Gizmo::SelectEntity(const ECSEntity a_entity, class EntityComponentSystem* a_ecs)
{
    m_ecs = a_ecs;
    m_selected_entity = a_entity;
    ClearCollision();
}

void Gizmo::UnselectEntity()
{
    m_ecs = nullptr;
    m_selected_entity = INVALID_ECS_OBJ;
    ClearCollision();
}

void Gizmo::SetMode(const GIZMO_MODE a_mode)
{
    m_mode = a_mode;
}

bool Gizmo::RayCollide(const float3 a_ray_origin, const float3 a_ray_dir)
{
    m_hit_flags = _RayCollide(a_ray_origin, a_ray_dir);
    return m_hit_flags != 0;
}

void Gizmo::ClearCollision()
{
    m_hit_flags = 0;
}

static void IsLineSelected(Line& a_line, const LineColor a_normal_color, const GIZMO_HIT_FLAGS a_hit, const GIZMO_HIT_FLAGS a_hit_flags)
{
    constexpr LineColor selected_color = LineColor(255, 255, 255, true);
    if (a_hit_flags == a_hit)
    {
        a_line.p0_color = selected_color;
        a_line.p1_color = selected_color;
    }
    else
    {
        a_line.p0_color = a_normal_color;
        a_line.p1_color = a_normal_color;
    }
}

void Gizmo::Draw() const
{
    constexpr LineColor right_color = LineColor(255, 0, 0, true);
    constexpr LineColor up_color = LineColor(0, 255, 0, true);
    constexpr LineColor forward_color = LineColor(0, 0, 255, true);

    const GizmoPosAndScale ps = GetGizmoPosAndScale(*m_ecs, m_selected_entity);
    if (m_mode == GIZMO_MODE::TRANSLATE || m_mode == GIZMO_MODE::SCALE)
    {
        FixedArray<Line, 9> lines;
        lines[0].p0 = ps.pos;
        lines[0].p1 = ps.pos + float3(ps.scale, 0.f, 0.f);
        IsLineSelected(lines[0], right_color, static_cast<GIZMO_HIT_FLAGS>(GIZMO_HIT::RIGHT), m_hit_flags);
        lines[1].p0 = ps.pos;
        lines[1].p1 = ps.pos + float3(0.f, ps.scale, 0.f);
        IsLineSelected(lines[1], up_color, static_cast<GIZMO_HIT_FLAGS>(GIZMO_HIT::UP), m_hit_flags);
        lines[2].p0 = ps.pos;
        lines[2].p1 = ps.pos + float3(0.f, 0.f, ps.scale);
        IsLineSelected(lines[2], forward_color, static_cast<GIZMO_HIT_FLAGS>(GIZMO_HIT::FORWARD), m_hit_flags);

        const float scale_size = GetQuadSize(ps.scale);
        constexpr GIZMO_HIT_FLAGS right_up = static_cast<GIZMO_HIT_FLAGS>(GIZMO_HIT::RIGHT) + static_cast<GIZMO_HIT_FLAGS>(GIZMO_HIT::UP);
        constexpr GIZMO_HIT_FLAGS up_forward = static_cast<GIZMO_HIT_FLAGS>(GIZMO_HIT::UP) + static_cast<GIZMO_HIT_FLAGS>(GIZMO_HIT::FORWARD);
        constexpr GIZMO_HIT_FLAGS forward_right = static_cast<GIZMO_HIT_FLAGS>(GIZMO_HIT::FORWARD) + static_cast<GIZMO_HIT_FLAGS>(GIZMO_HIT::RIGHT);

        lines[3].p0 = ps.pos + float3(scale_size, 0.f, 0.f);
        lines[3].p1 = ps.pos + float3(scale_size, scale_size, 0.f);
        lines[4].p0 = ps.pos + float3(0.f, scale_size, 0.f);
        lines[4].p1 = ps.pos + float3(scale_size, scale_size, 0.f);
        IsLineSelected(lines[3], right_color, right_up, m_hit_flags);
        IsLineSelected(lines[4], up_color, right_up, m_hit_flags);

        lines[5].p0 = ps.pos + float3(0.f, scale_size, 0.f);
        lines[5].p1 = ps.pos + float3(0.f, scale_size, scale_size);
        lines[6].p0 = ps.pos + float3(0.f, 0.f, scale_size);
        lines[6].p1 = ps.pos + float3(0.f, scale_size, scale_size);
        IsLineSelected(lines[5], up_color, up_forward, m_hit_flags);
        IsLineSelected(lines[6], forward_color, up_forward, m_hit_flags);

        lines[7].p0 = ps.pos + float3(0.f, 0.f, scale_size);
        lines[7].p1 = ps.pos + float3(scale_size, 0.f, scale_size);
        lines[8].p0 = ps.pos + float3(scale_size, 0.f, 0.f);
        lines[8].p1 = ps.pos + float3(scale_size, 0.f, scale_size);
        IsLineSelected(lines[7], forward_color, forward_right, m_hit_flags);
        IsLineSelected(lines[8], right_color, forward_right, m_hit_flags);

        m_ecs->AddLinesToFrame(lines.const_slice());
    }
    else
    {
        BB_UNIMPLEMENTED("gizmo rotation");
    }
}

void Gizmo::ManipulateEntity(const float2 a_mouse_move)
{
    if (m_hit_flags == 0)
        return;

    float3 change = float3();
    if (m_hit_flags & static_cast<GIZMO_HIT_FLAGS>(GIZMO_HIT::RIGHT))
        change.x = a_mouse_move.x;
    if (m_hit_flags & static_cast<GIZMO_HIT_FLAGS>(GIZMO_HIT::UP))
        change.y = -a_mouse_move.y;
    if (m_hit_flags & static_cast<GIZMO_HIT_FLAGS>(GIZMO_HIT::FORWARD))
        change.z = a_mouse_move.x;

    change = change * 0.01f;

    if (m_mode == GIZMO_MODE::TRANSLATE)
        m_ecs->Translate(m_selected_entity, change);
    else if (m_mode == GIZMO_MODE::SCALE)
        m_ecs->Scale(m_selected_entity, change);
    else
        BB_UNIMPLEMENTED("gizmo rotation");
}

bool Gizmo::HasValidEntity() const
{
    if (m_ecs == nullptr)
        return false;
    return m_ecs->ValidateEntity(m_selected_entity);
}

GIZMO_HIT_FLAGS Gizmo::_RayCollide(const float3 a_ray_origin, const float3 a_ray_dir) const
{
    const GizmoPosAndScale ps = GetGizmoPosAndScale(*m_ecs, m_selected_entity);

    if (m_mode == GIZMO_MODE::TRANSLATE || m_mode == GIZMO_MODE::SCALE)
    {
        const float scale_size = GetQuadSize(ps.scale);
        constexpr float line_box_off = 0.01f;

        if (BoxRayIntersect(ps.pos + float3(scale_size, -line_box_off, -line_box_off), ps.pos + float3(ps.scale, 0.f, 0.f) + float3(line_box_off), a_ray_origin, a_ray_dir))
            return static_cast<uint32_t>(GIZMO_HIT::RIGHT);
        if (BoxRayIntersect(ps.pos + float3(-line_box_off, scale_size, -line_box_off), ps.pos + float3(0.f, ps.scale, 0.f) + float3(line_box_off), a_ray_origin, a_ray_dir))
            return static_cast<uint32_t>(GIZMO_HIT::UP);
        if (BoxRayIntersect(ps.pos + float3(-line_box_off, -line_box_off, scale_size), ps.pos + float3(0.f, 0.f, ps.scale) + float3(line_box_off), a_ray_origin, a_ray_dir))
            return static_cast<uint32_t>(GIZMO_HIT::FORWARD);

        if (BoxRayIntersect(ps.pos, ps.pos + float3(scale_size, scale_size, 0.f), a_ray_origin, a_ray_dir))
            return static_cast<uint32_t>(GIZMO_HIT::RIGHT) | static_cast<uint32_t>(GIZMO_HIT::UP);
        if (BoxRayIntersect(ps.pos, ps.pos + float3(0.f, scale_size, scale_size), a_ray_origin, a_ray_dir))
            return static_cast<uint32_t>(GIZMO_HIT::UP) | static_cast<uint32_t>(GIZMO_HIT::FORWARD);
        if (BoxRayIntersect(ps.pos, ps.pos + float3(scale_size, 0.f, scale_size), a_ray_origin, a_ray_dir))
            return static_cast<uint32_t>(GIZMO_HIT::FORWARD) | static_cast<uint32_t>(GIZMO_HIT::RIGHT);
    }
    else
    {
        BB_UNIMPLEMENTED("gizmo rotation");
    }

    return 0;
}

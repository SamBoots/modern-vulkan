#include "Gizmo.hpp"
#include "ecs/EntityComponentSystem.hpp"
#include "Math/Math.inl"
#include "Math/Collision.inl"

using namespace BB;

static float GetQuadSize(const Gizmo& a_gizmo)
{
    constexpr float scale_size_len_mod = 0.25f;
    return a_gizmo.arrow_length * scale_size_len_mod;
}

Gizmo BB::CreateGizmo(EntityComponentSystem& a_ecs, const ECSEntity a_entity)
{
    const BoundingBox& box = a_ecs.GetBoundingBox(a_entity);
    const float4x4& world_mat = a_ecs.GetWorldMatrix(a_entity);
    const float3 scale = Float4x4ExtractScale(world_mat);
    const float3 mod_scale = scale * Float3Distance(box.min, box.max);
    const float line_length = (mod_scale.x + mod_scale.y + mod_scale.z) / 3;

    Gizmo gizmo;
    gizmo.pos = Float4x4ExtractTranslation(world_mat) + float3(0.f, mod_scale.y / 2, 0.f);;
    gizmo.arrow_length = line_length;

    return gizmo;
}

GIZMO_HIT_FLAGS BB::GizmoCollide(const Gizmo& a_gizmo, const float3 a_ray_origin, const float3 a_ray_dir)
{
    constexpr float line_box_size = 0.005f;
    if (BoxRayIntersect(a_gizmo.pos + float3(-line_box_size), a_gizmo.pos + float3(a_gizmo.arrow_length, 0.f, 0.f) + float3(line_box_size), a_ray_origin, a_ray_dir))
        return static_cast<uint32_t>(GIZMO_HIT::RIGHT);
    if (BoxRayIntersect(a_gizmo.pos + float3(-line_box_size), a_gizmo.pos + float3(0.f, a_gizmo.arrow_length, 0.f) + float3(line_box_size), a_ray_origin, a_ray_dir))
        return static_cast<uint32_t>(GIZMO_HIT::UP);
    if (BoxRayIntersect(a_gizmo.pos + float3(-line_box_size), a_gizmo.pos + float3(0.f, 0.f, a_gizmo.arrow_length) + float3(line_box_size), a_ray_origin, a_ray_dir))
        return static_cast<uint32_t>(GIZMO_HIT::FORWARD);

    const float scale_size = GetQuadSize(a_gizmo);

    if (BoxRayIntersect(a_gizmo.pos, float3(scale_size, scale_size, 0.f), a_ray_origin, a_ray_dir))
        return static_cast<uint32_t>(GIZMO_HIT::RIGHT) | static_cast<uint32_t>(GIZMO_HIT::UP);
    if (BoxRayIntersect(a_gizmo.pos, float3(0.f, scale_size, scale_size), a_ray_origin, a_ray_dir))
        return static_cast<uint32_t>(GIZMO_HIT::UP) | static_cast<uint32_t>(GIZMO_HIT::FORWARD);
    if (BoxRayIntersect(a_gizmo.pos, float3(scale_size, 0.f, scale_size), a_ray_origin, a_ray_dir))
        return static_cast<uint32_t>(GIZMO_HIT::FORWARD) | static_cast<uint32_t>(GIZMO_HIT::RIGHT);

    return 0;
}

static void IsLineSelected(Line& a_line, const LineColor a_normal_color, const GIZMO_HIT a_hit, const GIZMO_HIT_FLAGS a_hit_flags)
{
    constexpr LineColor selected_color = LineColor(255, 255, 255, true);
    if (a_hit_flags & static_cast<GIZMO_HIT_FLAGS>(a_hit))
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

void BB::DrawGizmo(class EntityComponentSystem& a_ecs, const Gizmo& a_gizmo, const GIZMO_HIT_FLAGS a_hits)
{
    constexpr LineColor right_color = LineColor(255, 0, 0, true);
    constexpr LineColor up_color = LineColor(0, 255, 0, true);
    constexpr LineColor forward_color = LineColor(0, 0, 255, true);

    FixedArray<Line, 9> lines;
    lines[0].p0 = a_gizmo.pos;
    lines[0].p1 = a_gizmo.pos + float3(a_gizmo.arrow_length, 0.f, 0.f);
    IsLineSelected(lines[0], right_color, GIZMO_HIT::RIGHT, a_hits);
    lines[1].p0 = a_gizmo.pos;
    lines[1].p1 = a_gizmo.pos + float3(0.f, a_gizmo.arrow_length, 0.f);
    IsLineSelected(lines[1], up_color, GIZMO_HIT::UP, a_hits);
    lines[2].p0 = a_gizmo.pos;
    lines[2].p1 = a_gizmo.pos + float3(0.f, 0.f, a_gizmo.arrow_length);
    IsLineSelected(lines[2], forward_color, GIZMO_HIT::FORWARD, a_hits);

    const float scale_size = GetQuadSize(a_gizmo);

    lines[3].p0 = a_gizmo.pos + float3(scale_size, 0.f, 0.f);
    lines[3].p1 = a_gizmo.pos + float3(scale_size, scale_size, 0.f);
    lines[4].p0 = a_gizmo.pos + float3(0.f, scale_size, 0.f);
    lines[4].p1 = a_gizmo.pos + float3(scale_size, scale_size, 0.f);
    IsLineSelected(lines[3], right_color, GIZMO_HIT::RIGHT, a_hits);
    IsLineSelected(lines[4], up_color, GIZMO_HIT::UP, a_hits);

    lines[5].p0 = a_gizmo.pos + float3(0.f, scale_size, 0.f);
    lines[5].p1 = a_gizmo.pos + float3(0.f, scale_size, scale_size);
    lines[6].p0 = a_gizmo.pos + float3(0.f, 0.f, scale_size);
    lines[6].p1 = a_gizmo.pos + float3(0.f, scale_size, scale_size);
    IsLineSelected(lines[5], up_color, GIZMO_HIT::UP, a_hits);
    IsLineSelected(lines[6], forward_color, GIZMO_HIT::FORWARD, a_hits);

    lines[7].p0 = a_gizmo.pos + float3(0.f, 0.f, scale_size);
    lines[7].p1 = a_gizmo.pos + float3(scale_size, 0.f, scale_size);
    lines[8].p0 = a_gizmo.pos + float3(scale_size, 0.f, 0.f);
    lines[8].p1 = a_gizmo.pos + float3(scale_size, 0.f, scale_size);
    IsLineSelected(lines[7], forward_color, GIZMO_HIT::FORWARD, a_hits);
    IsLineSelected(lines[8], right_color, GIZMO_HIT::RIGHT, a_hits);

    a_ecs.AddLinesToFrame(lines.const_slice());
}

void BB::GizmoManipulateEntity(class EntityComponentSystem& a_ecs, const ECSEntity a_entity, const GIZMO_HIT_FLAGS a_gizmo_hits, const float2 a_mouse_move)
{
    float3 translate = float3();
    if (a_gizmo_hits & static_cast<GIZMO_HIT_FLAGS>(GIZMO_HIT::RIGHT))
        translate.x = a_mouse_move.x;
    if (a_gizmo_hits & static_cast<GIZMO_HIT_FLAGS>(GIZMO_HIT::UP))
        translate.y = -a_mouse_move.y;
    if (a_gizmo_hits & static_cast<GIZMO_HIT_FLAGS>(GIZMO_HIT::FORWARD))
        translate.z = a_mouse_move.x;

    a_ecs.Translate(a_entity, translate);
}

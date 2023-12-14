#include "Transform.hpp"
#include "Math.inl"

#include "MemoryArena.hpp"

using namespace BB;

Transform::Transform(const float3 a_position)
	: Transform(a_position, float3{0,0,0}, 0, float3{1,1,1}) {}

Transform::Transform(const float3 a_position, const float3 a_axis, const float a_radians)
	: Transform(a_position, a_axis, a_radians, float3{1,1,1}) {}

Transform::Transform(const float3 a_position, const float3 a_axis, const float a_radians, const float3 a_scale)
	: m_pos(a_position), m_Scale(a_scale) 
{
	m_rot = QuatFromAxisAngle(a_axis, a_radians); //glm::angleAxis(glm::radians(a_radians), a_axis);
}

void Transform::Translate(const float3 a_translation)
{
	m_pos = m_pos + a_translation;
}

void Transform::Rotate(const float3 a_axis, const float a_radians)
{
	m_rot = m_rot * Quat{ a_axis.x,a_axis.y,a_axis.z, a_radians }; //glm::rotate(m_rot, a_radians, a_axis);
}

void Transform::SetPosition(const float3 a_position)
{
	m_pos = a_position;
}

void Transform::SetRotation(const float3 a_axis, const float a_radians)
{
	m_rot = QuatFromAxisAngle(a_axis, a_radians); //glm::angleAxis(a_radians, a_axis);
}

void Transform::SetScale(const float3 a_scale)
{
	m_Scale = a_scale;
}

const float4x4 Transform::CreateMatrix()
{
	float4x4 t_Matrix = Float4x4Identity();
	t_Matrix = t_Matrix * Float4x4FromTranslation(m_pos);
	t_Matrix = t_Matrix * Float4x4FromQuat(m_rot);
	t_Matrix = Float4x4Scale(t_Matrix, m_Scale);
	return t_Matrix;
}

//slotmap type of data structure.
struct BB::TransformNode
{
	union //44 bytes
	{
		Transform transform; 
		uint32_t next;
	};
	
	uint32_t generation; //48 bytes
};

TransformPool::TransformPool(MemoryArena& a_arena, const uint32_t a_transform_count)
{
	m_transform_count = a_transform_count;
	m_next_free_transform = 0;
	m_transforms = reinterpret_cast<TransformNode*>(ArenaAlloc(a_arena, a_transform_count * sizeof(TransformNode), __alignof(TransformNode)));
	for (size_t i = 0; i < static_cast<size_t>(m_transform_count - 1); i++)
	{
		m_transforms[i].next = static_cast<uint32_t>(i + 1);
		m_transforms[i].generation = 1;
	}

	m_transforms[m_transform_count - 1].next = UINT32_MAX;
	m_transforms[m_transform_count - 1].generation = 1;
}

TransformHandle TransformPool::CreateTransform(const float3 a_position)
{
	const uint32_t transform_index = m_next_free_transform;
	TransformNode* node = &m_transforms[transform_index];
	m_next_free_transform = node->next;

	//WILL OVERWRITE node->next due to it being a union.
	new (&node->transform) Transform(a_position);

	return TransformHandle(transform_index, node->generation);
}

TransformHandle TransformPool::CreateTransform(const float3 a_position, const float3 a_axis, const float a_radians)
{
	const uint32_t transform_index = m_next_free_transform;
	TransformNode* node = &m_transforms[transform_index];
	m_next_free_transform = node->next;

	//WILL OVERWRITE node->next due to it being a union.
	new (&node->transform) Transform(a_position, a_axis, a_radians);

	return TransformHandle(transform_index, node->generation);
}

TransformHandle TransformPool::CreateTransform(const float3 a_position, const float3 a_axis, const float a_radians, const float3 a_scale)
{
	const uint32_t transform_index = m_next_free_transform;
	TransformNode* node = &m_transforms[transform_index];
	m_next_free_transform = node->next;

	//WILL OVERWRITE node->next due to it being a union.
	new (&node->transform) Transform(a_position, a_axis, a_radians, a_scale);

	return TransformHandle(transform_index, node->generation);
}

void TransformPool::FreeTransform(const TransformHandle a_handle)
{
	BB_ASSERT(a_handle.extra_index == m_transforms[a_handle.index].generation, "Transform likely freed twice.");

	//mark transform as free.
	m_transforms[a_handle.index].next = m_transforms->next;
	++m_transforms[a_handle.index].generation;
	m_transforms->next = a_handle.index;
}

Transform& TransformPool::GetTransform(const TransformHandle a_handle) const
{
	BB_ASSERT(a_handle.extra_index == m_transforms[a_handle.index].generation, "Transform likely freed twice.");
	return m_transforms[a_handle.index].transform;
}

uint32_t TransformPool::PoolSize() const
{
	return m_transform_count;
}

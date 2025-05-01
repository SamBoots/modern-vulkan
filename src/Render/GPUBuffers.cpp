#include "GPUBuffers.hpp"
#include "VulkanRenderer.hpp"
#include "Program.h"

using namespace BB;

void GPUStaticCPUWriteableBuffer::Init(const BUFFER_TYPE a_buffer_type, const size_t a_size, const StringView a_name)
{
	GPUBufferCreateInfo create_info;
	create_info.name = a_name.c_str();
	create_info.host_writable = true;
	create_info.size = a_size;
	create_info.type = a_buffer_type;
	m_buffer = Vulkan::CreateBuffer(create_info);

	m_size = a_size;
	m_mapped_memory = Vulkan::MapBufferMemory(m_buffer);
}

void GPUStaticCPUWriteableBuffer::Destroy()
{
	Vulkan::UnmapBufferMemory(m_buffer);
	Vulkan::FreeBuffer(m_buffer);
}

bool GPUStaticCPUWriteableBuffer::WriteTo(void* a_data, const size_t a_size, const uint32_t a_offset)
{
	if (a_size + a_offset > m_size)
	{
		BB_ASSERT(false, "writing out of bounds in CPU writeable GPU buffer");
		return false;
	}

	memcpy(Pointer::Add(m_mapped_memory, a_offset), a_data, a_size);
	return true;
}

const GPUBufferView GPUStaticCPUWriteableBuffer::GetView() const
{
	GPUBufferView view;
	view.buffer = m_buffer;
	view.size = m_size;
	view.offset = 0;
	return view;
}

void GPULinearBuffer::Init(const GPUBufferCreateInfo& a_buffer_info)
{
	m_buffer = Vulkan::CreateBuffer(a_buffer_info);
	m_capacity = a_buffer_info.size;
	m_size = 0;
}

bool GPULinearBuffer::Allocate(const size_t a_byte_amount, GPUBufferView& a_out_view)
{
	const size_t offset = m_size.fetch_add(a_byte_amount, std::memory_order_relaxed);
	if (m_capacity < offset)
	{
		return false;
	}

	a_out_view.buffer = m_buffer;
	a_out_view.offset = offset;
	a_out_view.size = a_byte_amount;

	return true;
}

void GPULinearBuffer::Clear()
{
	m_size = 0;
}

void GPUUploadRingAllocator::Init(MemoryArena& a_arena, const size_t a_ring_buffer_size, const RFence a_fence, const char* a_name)
{
	GPUBufferCreateInfo create_info;
	create_info.type = BUFFER_TYPE::UPLOAD;
	create_info.size = a_ring_buffer_size;
	create_info.name = a_name;
	create_info.host_writable = true;

	m_lock = OSCreateRWLock();
	m_fence = a_fence;
	m_buffer = Vulkan::CreateBuffer(create_info);

	m_begin = Vulkan::MapBufferMemory(m_buffer);
	m_end = Pointer::Add(m_begin, a_ring_buffer_size);
	m_head = 0;
	m_tail = 0;
	m_size = 0;

	m_locked_queue.Init(a_arena, RING_BUFFER_QUEUE_ELEMENT_COUNT);
}

uint64_t GPUUploadRingAllocator::AllocateUploadMemory(const size_t a_byte_amount, const GPUFenceValue a_fence_value)
{
	BB_ASSERT(a_byte_amount < Capacity(), "trying to upload more memory then the ringbuffer size");
	BBRWLockScopeWrite scope_lock(m_lock);

	if (m_locked_queue.IsFull())
	{
		FreeElements();
		if (m_locked_queue.IsFull())
			return uint64_t(-1);
	}

	size_t offset = FindOffset(a_byte_amount, a_fence_value);

	if (offset == size_t(-1))
	{
		FreeElements();
		offset = FindOffset(a_byte_amount, a_fence_value);
		if (offset == size_t(-1))
			return uint64_t(-1);
	}

	return offset;
}

bool GPUUploadRingAllocator::MemcpyIntoBuffer(const size_t a_offset, const void* a_src_data, const size_t a_src_size) const
{
	if (a_offset + a_src_size > Capacity())
		return false;

	memcpy(Pointer::Add(m_begin, a_offset), a_src_data, a_src_size);
	return true;
}

size_t GPUUploadRingAllocator::FindOffset(const size_t a_byte_amount, const GPUFenceValue a_fence_value)
{
	GPUUploadRingAllocator::LockedRegions locked_region;
	locked_region.fence_value = a_fence_value;

	if (m_size == Capacity())
		return size_t(-1);

	if (m_tail >= m_head)
	{
		if (m_tail + a_byte_amount <= Capacity())
		{
			const size_t offset = m_tail;
			m_tail += a_byte_amount;
			m_size += a_byte_amount;

			locked_region.begin = m_tail;
			locked_region.size = a_byte_amount;
			m_locked_queue.EnQueue(locked_region);
			return offset;
		}
		else if (a_byte_amount <= m_head)
		{
			const size_t padding = Capacity() - m_tail;
			m_size += padding + a_byte_amount;
			m_tail = a_byte_amount;

			locked_region.begin = m_tail;
			locked_region.size = padding + a_byte_amount;
			m_locked_queue.EnQueue(locked_region);
			return 0;
		}
	}
	else if (m_tail + m_size <= m_head)
	{
		const size_t offset = m_tail;
		m_tail += a_byte_amount;
		m_size += a_byte_amount;

		locked_region.begin = m_tail;
		locked_region.size = a_byte_amount;
		m_locked_queue.EnQueue(locked_region);
		return offset;
	}

	return size_t(-1);
}

void GPUUploadRingAllocator::FreeElements()
{
	const GPUFenceValue fence_value = Vulkan::GetCurrentFenceValue(m_fence);

	while (const LockedRegions* locked_region = m_locked_queue.Peek())
	{
		if (locked_region->fence_value <= fence_value)
		{
			m_size -= locked_region->size;
			m_head = locked_region->begin;
			m_locked_queue.DeQueue();
		}
		else
			break;
	}
}

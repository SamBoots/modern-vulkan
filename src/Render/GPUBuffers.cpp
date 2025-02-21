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
	const size_t offset = m_size.fetch_add(a_byte_amount);
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
	m_write_at = m_begin;
	m_end = Pointer::Add(m_begin, a_ring_buffer_size);
	m_free_until = m_end;
	m_locked_queue.Init(a_arena, RING_BUFFER_QUEUE_ELEMENT_COUNT);
}

UploadBuffer GPUUploadRingAllocator::AllocateUploadMemory(const size_t a_byte_amount, const uint64_t a_fence_value)
{
	BB_ASSERT(a_byte_amount < GetUploadAllocatorCapacity(), "trying to upload more memory then the ringbuffer size");
	BBRWLockScopeWrite scope_lock(m_lock);

	void* begin = m_write_at;
	void* end = Pointer::Add(m_write_at, a_byte_amount);

	bool must_free_memory = end > m_free_until ? true : false;

	// if we go over the end, but not over the readpointer then recalculate
	if (end > m_end)
	{
		m_write_at = begin;
		begin = m_begin;
		end = Pointer::Add(m_begin, a_byte_amount);

	}
	size_t remaining_size = GetUploadSpaceRemaining();
	if (m_locked_queue.IsFull() || a_byte_amount > remaining_size)
		must_free_memory = true;

	if (must_free_memory)
	{
		const uint64_t fence_value = Vulkan::GetCurrentFenceValue(m_fence);

		while (const LockedRegions* locked_region = m_locked_queue.Peek())
		{
			if (locked_region->fence_value <= fence_value)
			{
				m_free_until = locked_region->memory_end;
				m_locked_queue.DeQueue();
			}
			else
				break;
		}

		remaining_size = GetUploadSpaceRemaining();
		if (a_byte_amount > remaining_size || m_locked_queue.IsFull())
			return UploadBuffer();
	}

	GPUUploadRingAllocator::LockedRegions locked_region;
	locked_region.memory_end = end;
	locked_region.fence_value = a_fence_value;
	m_locked_queue.EnQueue(locked_region);
	m_write_at = end;

	UploadBuffer upload_buffer;
	upload_buffer.buffer = m_buffer;
	upload_buffer.begin = begin;
	upload_buffer.end = end;
	upload_buffer.base_offset = reinterpret_cast<size_t>(Pointer::Subtract(begin, reinterpret_cast<size_t>(m_begin)));
	return upload_buffer;
}

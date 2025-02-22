#pragma once

#include "Rendererfwd.hpp"
#include "Storage/BBString.h"
#include "Storage/Queue.hpp"
#include <atomic>

namespace BB
{
	class GPUStaticCPUWriteableBuffer
	{
	public:
		void Init(const BUFFER_TYPE buffer_type, const size_t a_size, const StringView a_name);
		void Destroy();

		bool WriteTo(void* a_data, const size_t a_size, const uint32_t a_offset);
		const GPUBufferView GetView() const;
		const GPUBuffer GetBuffer() const { return m_buffer; }

	private:
		GPUBuffer m_buffer;
		size_t m_size;
		void* m_mapped_memory;
	};

	// THREAD SAFE
	class GPULinearBuffer
	{
	public:
		void Init(const GPUBufferCreateInfo& a_buffer_info);
		// THREAD SAFE

		bool Allocate(const size_t a_byte_amount, GPUBufferView& a_out_view);
		void Clear();

		const GPUBuffer GetBuffer() const { return m_buffer; }
	private:
		GPUBuffer m_buffer;
		size_t m_capacity;
		std::atomic<size_t> m_size;
	};

	struct UploadBuffer
	{
		// returns a_dst_offset + a_src_size
		size_t SafeMemcpy(const size_t a_dst_offset, const void* a_src, const size_t a_src_size) const
		{
			if (a_src_size)
			{
				void* copy_pos = Pointer::Add(begin, a_dst_offset);
				BB_ASSERT(Pointer::Add(copy_pos, a_src_size) <= end, "gpu upload buffer writing out of bounds");

				memcpy(copy_pos, a_src, a_src_size);
			}
			return a_dst_offset + a_src_size;
		}

		GPUBuffer buffer;
		void* begin;
		void* end;
		size_t base_offset;
	};

	constexpr size_t RING_BUFFER_QUEUE_ELEMENT_COUNT = 128;

	class GPUUploadRingAllocator
	{
	public:
		void Init(MemoryArena& a_arena, const size_t a_ring_buffer_size, const RFence a_fence, const char* a_name);

		UploadBuffer AllocateUploadMemory(const size_t a_byte_amount, const uint64_t a_fence_value);

		size_t GetUploadAllocatorCapacity() const
		{
			return reinterpret_cast<size_t>(m_end) - reinterpret_cast<size_t>(m_begin);
		}

		size_t GetUploadSpaceRemaining() const
		{
			return (m_free_until > m_write_at) ? reinterpret_cast<size_t>(m_free_until) - reinterpret_cast<size_t>(m_write_at) : 
				GetUploadAllocatorCapacity() - reinterpret_cast<size_t>(m_write_at) + reinterpret_cast<size_t>(m_free_until);
		}

		const GPUBuffer GetBuffer() const { return m_buffer; }
		const RFence GetFence() const { return m_fence; }

	private:
		struct LockedRegions
		{
			void* memory_end;
			uint64_t fence_value;
		};

		BBRWLock m_lock;
		RFence m_fence;
		GPUBuffer m_buffer;

		void* m_begin;
		void* m_free_until;
		void* m_write_at;
		void* m_end;
		SPSCQueue<LockedRegions> m_locked_queue;
	};
}

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

	constexpr size_t RING_BUFFER_QUEUE_ELEMENT_COUNT = 128;

	// idea from https://www.codeproject.com/Articles/1094799/Implementing-Dynamic-Resources-with-Direct3D12
	class GPUUploadRingAllocator
	{
	public:
		void Init(MemoryArena& a_arena, const size_t a_ring_buffer_size, const RFence a_fence, const char* a_name);

		uint64_t AllocateUploadMemory(const size_t a_byte_amount, const GPUFenceValue a_fence_value, const bool a_retry = true);

		bool MemcpyIntoBuffer(const size_t a_offset, const void* a_src_data, const size_t a_src_size) const;
		size_t Capacity() const
		{
			return reinterpret_cast<size_t>(m_end) - reinterpret_cast<size_t>(m_begin);
		}

		size_t SizeRemaining() const
		{
			return Capacity() - m_size;
		}

		const GPUBuffer GetBuffer() const { return m_buffer; }
		const RFence GetFence() const { return m_fence; }

	private:
		size_t FindOffset(const size_t a_byte_amount, const GPUFenceValue a_fence_value);
		void FreeElements();

		struct LockedRegions
		{
			size_t size;
			size_t begin;
			GPUFenceValue fence_value;
		};

		BBRWLock m_lock;
		RFence m_fence;
		GPUBuffer m_buffer;
		size_t m_size;

		void* m_begin;
		void* m_end;
		size_t m_head;
		size_t m_tail;

		SPSCQueue<LockedRegions> m_locked_queue;
	};
}

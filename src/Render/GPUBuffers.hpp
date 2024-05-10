#pragma once

#include "Rendererfwd.hpp"
#include "Storage/Queue.hpp"
#include <atomic>

namespace BB
{
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
}

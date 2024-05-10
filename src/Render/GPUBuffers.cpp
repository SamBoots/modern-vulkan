#include "GPUBuffers.hpp"
#include "VulkanRenderer.hpp"

using namespace BB;

void GPULinearBuffer::Init(const GPUBufferCreateInfo& a_buffer_info)
{
	m_buffer = Vulkan::CreateBuffer(a_buffer_info);
	m_capacity = a_buffer_info.size;
	m_size = 0;
}

bool GPULinearBuffer::Allocate(const size_t a_byte_amount, GPUBufferView& a_out_view)
{
	const size_t offset = m_size.fetch_add(a_byte_amount);
	if (m_capacity >= offset)
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

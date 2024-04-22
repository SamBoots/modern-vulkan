#pragma once
#include "BBMemory.h"
#include "Utils/Logger.h"

namespace BB
{
	struct MemoryArena;

	class BBImage
	{
	public:
		void Init(MemoryArena& a_arena, const char* a_file_path, const uint32_t a_bytes_per_pixel = 4);
		void Init(MemoryArena& a_arena, const BBImage& a_image);
		void Init(MemoryArena& a_arena, void* a_pixels_to_copy, const uint32_t a_width, const uint32_t a_height, const uint32_t a_bytes_per_pixel);

		void FilterImage(MemoryArena& a_temp_arena,
			const float* a_filter, 
			const uint32_t a_filter_width, 
			const uint32_t a_filter_height, 
			const float a_factor, 
			const float a_bias, 
			const uint32_t a_thread_count);
		void SharpenImage(MemoryArena& a_temp_arena, const float a_factor, const uint32_t a_thread_count);

		void WriteAsBMP(const char* a_file_path);
		void WriteAsTARGA(const char* a_file_path);

		uint32_t GetWidth() const { return m_width; }
		uint32_t GetHeight() const { return m_height; }
		uint32_t GetBytesPerPixel() const { return m_bytes_per_pixel; }
		const void* GetPixels() const { return reinterpret_cast<const void*>(m_pixels); }

	private:
		uint32_t m_bytes_per_pixel;
		uint32_t m_width;
		uint32_t m_height;

		void* m_pixels;

		void LoadBMP(MemoryArena& a_arena, const char* a_file_path, const uint32_t a_bytes_per_pixel);
	};
}

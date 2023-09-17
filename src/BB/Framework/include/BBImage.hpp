#pragma once
#include "BBMemory.h"
#include "Utils/Logger.h"

namespace BB
{
	class BBImage
	{
	public:
		BBImage(Allocator a_allocator, const char* a_file_path);
		BBImage(Allocator a_allocator, const uint32_t a_width, const uint32_t a_height, const bool has_alpha);
		BBImage(Allocator a_allocator, const BBImage& a_image);

		BBImage& operator=(const BBImage& a_Rhs);

		void FilterImage(Allocator a_temp_allocator, const float* a_filter, const uint32_t a_filter_width, const uint32_t a_filter_height, const float a_factor, const float a_bias, const uint32_t a_thread_count);
		void SharpenImage(Allocator a_temp_allocator, const float a_intensity, const uint32_t a_thread_count);

		void WriteAsBMP(const char* a_file_path);
		void WriteAsTARGA(const char* a_file_path);

	private:
		static const uint32_t m_bit_count = 32;
		uint32_t m_width;
		uint32_t m_height;

		union RGBA_Pixel
		{
			uint32_t rgba;
			struct
			{
				uint8_t r, g, b, a;
			};
		};
		RGBA_Pixel* m_pixels;

		void LoadBMP(Allocator a_allocator, const char* a_file_path);
	};
}
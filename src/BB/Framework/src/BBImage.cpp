#include "BBImage.hpp"
#include "OS/Program.h"
#include "BBThreadScheduler.hpp"

#include "Math.inl"

#include "MemoryArena.hpp"

#define PACK_RGBA(r, g, b, a) static_cast<uint32_t>((static_cast<uint32_t>(r)<<24|static_cast<uint32_t>(g)<<16|static_cast<uint32_t>(b)<<8|static_cast<uint32_t>(a)))

using namespace BB;

namespace BMP
{
	constexpr uint16_t BMP_IMAGE_FILE_TYPE = 0x4D42;
	constexpr uint32_t BMP_RGBA_COLOR_SPACE = 0x73524742;
#pragma pack(push, 1)
	struct File
	{
		uint16_t file_type; //2
		uint32_t file_size; //6
		uint16_t reserved1; //8
		uint16_t reserved2; //10
		uint32_t offset_data; //14
	};
	struct Header
	{
		uint32_t size; //4
		int32_t width; //8
		int32_t height; //12
		uint16_t planes; //14
		uint16_t bit_count; //16
		uint32_t compression; //20
		uint32_t size_image; //24
		int32_t x_pixels_per_meter; //28
		int32_t y_pixels_per_meter; //32
		uint32_t colors_used; //36
		uint32_t colors_important; //40
	};
	struct ColorHeader
	{
		uint32_t red_mask; //4
		uint32_t green_mask; //8 
		uint32_t blue_mask; //12
		uint32_t alpha_mask; //16
		uint32_t color_space_type; //20
		uint32_t unused[16]; //84
	};
#pragma pack(pop)


	static bool isRGBA(const ColorHeader& a_color_header)
	{
		ColorHeader rgb;
		rgb.red_mask = 0x00ff0000;
		rgb.green_mask = 0x0000ff00;
		rgb.blue_mask = 0x000000ff;
		rgb.alpha_mask = 0xff000000;
		rgb.color_space_type = BMP_RGBA_COLOR_SPACE;

		if (a_color_header.color_space_type != rgb.color_space_type)
		{
			return false;
		}

		if (a_color_header.red_mask != rgb.red_mask ||
			a_color_header.green_mask != rgb.green_mask ||
			a_color_header.blue_mask != rgb.blue_mask ||
			a_color_header.alpha_mask != rgb.alpha_mask)
		{
			BB_ASSERT(false, "BMP color header is not RGBA while the color space type is assigned as RGBA!");
			return false;
		}
		return true;
	}
}

namespace TARGA
{
#pragma pack(push, 1)
	struct File
	{
		int8_t  id_field_length;
		int8_t  color_map_type;
		int8_t  image_type;
		struct ColorMap
		{
			int16_t color_map_origin;
			int16_t color_map_length;
			int8_t  color_map_depth;
		} color_map;
		struct Image
		{
			int16_t x_origin;
			int16_t y_origin;
			int16_t width;
			int16_t height;
			int8_t  bits_per_pixel;
			int8_t  image_descriptor;
		} image;
	};
#pragma pack(pop)
}

void BBImage::Init(MemoryArena& a_arena, const char* a_file_path, const uint32_t a_bytes_per_pixel)
{
	LoadBMP(a_arena, a_file_path, a_bytes_per_pixel);
}

void BBImage::Init(MemoryArena& a_arena, const BBImage& a_image)
{
	m_width = a_image.m_width;
	m_height = a_image.m_height;
	m_bytes_per_pixel = a_image.m_bytes_per_pixel;

	const uint32_t image_size = m_width * m_height * m_bytes_per_pixel;
	m_pixels = ArenaAlloc(a_arena, image_size, alignof(size_t));
	memcpy(m_pixels, a_image.m_pixels, image_size);
}

void BBImage::Init(MemoryArena& a_arena, void* a_pixels_to_copy, const uint32_t a_width, const uint32_t a_height, const uint32_t a_bytes_per_pixel)
{
	BB_ASSERT(a_bytes_per_pixel == 4 || a_bytes_per_pixel == 8, "not supporting non 4 bytes per pixels images now");
	m_width = a_width;
	m_height = a_height;
	m_bytes_per_pixel = a_bytes_per_pixel;

	const uint32_t image_size = m_width * m_height * m_bytes_per_pixel;
	m_pixels = ArenaAlloc(a_arena, image_size, alignof(size_t));
	memcpy(m_pixels, a_pixels_to_copy, image_size);
}

struct process_image_part_params
{
	Threads::Barrier* barrier;

	const uint32_t* img_start_old_pixel;
	uint32_t* img_start_new_pixel;
	uint32_t img_height_start;
	uint32_t img_height_end;
	uint32_t img_width;
	uint32_t img_height;

	const float* filter;
	uint32_t filter_width; 
	uint32_t filter_height;
	float factor;
	float bias;
};

static void FilterImagePart(MemoryArena&, void* a_param)
{
	const process_image_part_params& params = *reinterpret_cast<process_image_part_params*>(a_param);

	const __m128 simd_factor = _mm_set_ps1(params.factor);
	const __m128 simd_bias = _mm_set_ps1(params.bias);
	const __m128 simd_rgba_min = _mm_setzero_ps();
	const __m128 simd_rgba_max = _mm_set_ps1(255.f);

	for (uint32_t y = params.img_height_start; y < params.img_height_end; y++)
		for (uint32_t x = 0; x < params.img_width; x++)
		{
			__m128 simd_rgba = _mm_setzero_ps();

			for (uint32_t filterY = 0; filterY < params.filter_height; filterY++)
				for (uint32_t filterX = 0; filterX < params.filter_width; filterX++)
				{
					const uint32_t imageX = (x - params.filter_width / 2 + filterX + params.img_width) % params.img_width;
					const uint32_t imageY = (y - params.filter_height / 2 + filterY + params.img_height) % params.img_height;
					const uint32_t pixel = reinterpret_cast<const uint32_t*>(params.img_start_old_pixel)[imageY * params.img_width + imageX];

					const __m128 simd_filter = _mm_set_ps1(params.filter[filterY * params.filter_width + filterX]);
					const __m128 rgba_mod = _mm_set_ps(
						static_cast<float>((pixel & 0x000000ff) >> 0),
						static_cast<float>((pixel & 0x0000ff00) >> 8),
						static_cast<float>((pixel & 0x00ff0000) >> 16),
						static_cast<float>((pixel & 0xff000000) >> 24));

					simd_rgba = _mm_add_ps(simd_rgba, _mm_mul_ps(rgba_mod, simd_filter));
				}
			simd_rgba = _mm_mul_ps(simd_factor, simd_rgba);
			simd_rgba = _mm_min_ps(_mm_max_ps(_mm_add_ps(simd_rgba, simd_bias), simd_rgba_min), simd_rgba_max);

			const uint32_t packed = PACK_RGBA(
				reinterpret_cast<uint32_t*>(&simd_rgba)[0],
				reinterpret_cast<uint32_t*>(&simd_rgba)[1],
				reinterpret_cast<uint32_t*>(&simd_rgba)[2],
				reinterpret_cast<uint32_t*>(&simd_rgba)[3]);
			reinterpret_cast<uint32_t*>(params.img_start_new_pixel)[y * params.img_width + x] = packed;
		}


	//signal that we are done.
	params.barrier->Signal();
}

void BBImage::FilterImage(MemoryArena& a_temp_arena, const float* a_filter, const uint32_t a_filter_width, const uint32_t a_filter_height, const float a_factor, const float a_bias, const uint32_t a_thread_count)
{
	const uint32_t pixel_count = m_width * m_height * m_bytes_per_pixel;
	void* old_data = ArenaAlloc(a_temp_arena, pixel_count, alignof(size_t));
	memcpy(old_data, m_pixels, pixel_count);

	if (a_thread_count > 1)
	{
		const uint32_t pixel_height_per_thread = m_height / a_thread_count;
		const uint32_t worker_threads = a_thread_count - 1;

		//allocate on heap for better memory locality :) maybe.
		Threads::Barrier* thread_barrier = ArenaAllocType(a_temp_arena, Threads::Barrier)(a_thread_count);

		process_image_part_params param;
		param.barrier = thread_barrier;
		param.img_start_new_pixel = reinterpret_cast<uint32_t*>(m_pixels);
		param.img_start_old_pixel = reinterpret_cast<const uint32_t*>(old_data);
		param.img_width = m_width;
		param.img_height = m_height;

		param.filter = a_filter;
		param.filter_width = a_filter_width;
		param.filter_height = a_filter_height;
		param.factor = a_factor;
		param.bias = a_bias;

		for (uint32_t i = 0; i < worker_threads; i++)
		{
			param.img_height_start = pixel_height_per_thread * i;
			param.img_height_end = param.img_height_start + pixel_height_per_thread;

			Threads::StartTaskThread(FilterImagePart, &param, sizeof(param));
		}

		param.img_height_start = pixel_height_per_thread * worker_threads;
		param.img_height_end = param.img_height_start + pixel_height_per_thread;
		//now send the main thread for some work.
		FilterImagePart(a_temp_arena, &param);

		thread_barrier->Wait();
	}
	else
	{
		const VecFloat4 simd_factor = LoadFloat4(a_factor);
		const VecFloat4 simd_bias = LoadFloat4(a_bias);
		const VecFloat4 simd_rgba_min = LoadFloat4Zero();
		const VecFloat4 simd_rgba_max = LoadFloat4(255.f);

		for (uint32_t y = 0; y < m_height; y++)
			for (uint32_t x = 0; x < m_width; x++)
			{
				VecFloat4 simd_rgba = LoadFloat4Zero();

				for (uint32_t filterY = 0; filterY < a_filter_height; filterY++)
					for (uint32_t filterX = 0; filterX < a_filter_width; filterX++)
					{
						const uint32_t imageX = (x - a_filter_width / 2 + filterX + m_width) % m_width;
						const uint32_t imageY = (y - a_filter_height / 2 + filterY + m_height) % m_height;
						const uint32_t pixel = reinterpret_cast<uint32_t*>(old_data)[imageY * m_width + imageX];

						const VecFloat4 simd_filter = LoadFloat4(a_filter[filterY * a_filter_width + filterX]);
						const VecFloat4 rgba_mod = LoadFloat4(
							static_cast<float>((pixel & 0x000000ff) >> 0),
							static_cast<float>((pixel & 0x0000ff00) >> 8),
							static_cast<float>((pixel & 0x00ff0000) >> 16),
							static_cast<float>((pixel & 0xff000000) >> 24));

						simd_rgba = MulFloat4(AddFloat4(simd_rgba, rgba_mod), simd_filter);
					}

				simd_rgba = MulFloat4(simd_factor, simd_rgba);
				simd_rgba = MinFloat4(MaxFloat4(AddFloat4(simd_rgba, simd_bias), simd_rgba_min), simd_rgba_max);

				const uint32_t packed = PACK_RGBA(
					reinterpret_cast<uint32_t*>(&simd_rgba)[0],
					reinterpret_cast<uint32_t*>(&simd_rgba)[1],
					reinterpret_cast<uint32_t*>(&simd_rgba)[2],
					reinterpret_cast<uint32_t*>(&simd_rgba)[3]);
				reinterpret_cast<uint32_t*>(m_pixels)[y * m_width + x] = packed;
			}
	}
}

void BBImage::SharpenImage(MemoryArena& a_temp_arena, const float a_factor, const uint32_t a_thread_count)
{
	constexpr float sharpen3x3_bias = 0;
	constexpr float sharpen3x3_filter[3 * 3]
	{
		-1, -1, -1,
		-1,  9, -1,
		-1, -1, -1
	};
	FilterImage(a_temp_arena, sharpen3x3_filter, 3, 3, a_factor, sharpen3x3_bias, a_thread_count);
}

void BBImage::WriteAsBMP(const char* a_file_path)
{
	const size_t data_size = static_cast<size_t>(m_width) * static_cast<size_t>(m_height) * m_bytes_per_pixel;
	const OSFileHandle write_bmp = OSCreateFile(a_file_path);

	BMP::File file = {};
	file.file_type = BMP::BMP_IMAGE_FILE_TYPE;
	file.offset_data = sizeof(BMP::File) + sizeof(BMP::Header) + sizeof(BMP::ColorHeader);
	file.file_size = static_cast<uint32_t>(file.offset_data + data_size);

	BMP::Header file_header = {};
	file_header.size = sizeof(BMP::Header) + sizeof(BMP::ColorHeader);
	file_header.width = static_cast<int32_t>(m_width);
	file_header.height = static_cast<int32_t>(m_height);
	file_header.planes = 1; // this should be one at all times? // https://devblogs.microsoft.com/oldnewthing/20041201-00/?p=37163
	file_header.bit_count = static_cast<uint16_t>(m_bytes_per_pixel * 8);


	BMP::ColorHeader color_header = {};
	color_header.red_mask = 0x00ff0000;
	color_header.green_mask = 0x0000ff00;
	color_header.blue_mask = 0x000000ff;
	color_header.alpha_mask = 0xff000000;
	color_header.color_space_type = BMP::BMP_RGBA_COLOR_SPACE;

	OSWriteFile(write_bmp, &file, sizeof(BMP::File));
	OSWriteFile(write_bmp, &file_header, sizeof(BMP::Header));
	OSWriteFile(write_bmp, &color_header, sizeof(BMP::ColorHeader));
	OSWriteFile(write_bmp, m_pixels, data_size);

	CloseOSFile(write_bmp);
}

void BBImage::WriteAsTARGA(const char* a_file_path)
{
	const size_t data_size = static_cast<size_t>(m_width) * static_cast<size_t>(m_height) * sizeof(m_bytes_per_pixel);
	const OSFileHandle write_targa = OSCreateFile(a_file_path);

	TARGA::File file = {};
	file.color_map_type = 0;
	file.image_type = 2;
	file.color_map.color_map_origin = 0;
	file.image.x_origin = 0;
	file.image.y_origin = 0;
	file.image.width = static_cast<int16_t>(m_width);
	file.image.height = static_cast<int16_t>(m_height);
	file.image.bits_per_pixel = static_cast<int8_t>(m_bytes_per_pixel * 8);
	file.image.image_descriptor = 0;

	OSWriteFile(write_targa, &file, sizeof(TARGA::File));
	OSWriteFile(write_targa, m_pixels, data_size);

	CloseOSFile(write_targa);
}

void BBImage::LoadBMP(MemoryArena& a_arena, const char* a_file_path, const uint32_t a_bytes_per_pixel)
{
	BB_ASSERT(a_bytes_per_pixel == 4, "a_byts_per_pixel must be 3 or 4");
	m_bytes_per_pixel = a_bytes_per_pixel;
	const OSFileHandle image_file = OSLoadFile(a_file_path);
	BB_ASSERT(image_file.IsValid(), "failed to load image");

	BMP::File file;
	OSReadFile(image_file, &file, sizeof(file));

	BB_ASSERT(file.file_type == 0x4D42, "tried to load a file that is not a BMP image!");

	BMP::Header file_header;
	OSReadFile(image_file, &file_header, sizeof(file_header));

	BB_ASSERT(file_header.height > 0, "currently not supporting negative height BMP's.");
	BB_ASSERT(file_header.bit_count == 24 || file_header.bit_count == 32 || file_header.bit_count == 64, "unsupported bit_count");

	m_width = static_cast<uint32_t>(file_header.width);
	m_height = static_cast<uint32_t>(file_header.height);
	const uint32_t image_size = m_width * m_height * m_bytes_per_pixel;


	m_pixels = ArenaAlloc(a_arena, image_size, alignof(size_t));

	//uint32_t red_mask = 0x00ff0000;
	//uint32_t green_mask = 0x0000ff00;
	//uint32_t blue_mask = 0x000000ff;
	//uint32_t alpha_mask = 0xff000000;
	const bool has_color_header = file_header.size >= (sizeof(BMP::Header) + sizeof(BMP::ColorHeader));
	bool is_rgba = false;
	if (has_color_header)
	{
		BMP::ColorHeader color_header;
		OSReadFile(image_file, &color_header, sizeof(color_header));
		BB_WARNING(is_rgba = BMP::isRGBA(color_header), "non-RGBA bitmaps, still in testing", WarningType::MEDIUM);

		//red_mask = color_header.red_mask;
		//green_mask = color_header.green_mask;
		//blue_mask = color_header.blue_mask;
		//alpha_mask = color_header.alpha_mask;
	}
	SetOSFilePosition(image_file, file.offset_data, OS_FILE_READ_POINT::BEGIN);
	MemoryArenaScope(a_arena)
	{
		void* file_pixels = ArenaAlloc(a_arena, image_size, alignof(size_t));
		OSReadFile(image_file, file_pixels, image_size);

		//check to account for padding when not doing 32 or 64
		if (file_header.bit_count != 24)
		{
			memcpy(m_pixels, file_pixels, image_size);
		}
		else
		{
#pragma pack(push, 1)
			struct bit24pixel
			{
				uint8_t r;
				uint8_t g;
				uint8_t b;
			};
#pragma pack(pop)

			const bit24pixel* source_pixel = reinterpret_cast<const bit24pixel*>(file_pixels);
			uint8_t* write_pixels = reinterpret_cast<uint8_t*>(m_pixels);
			const uint32_t pixel_count = m_width * m_height;
			for (uint32_t i = 0; i < pixel_count; i++)
			{
				write_pixels[0] = source_pixel[i].r;
				write_pixels[1] = source_pixel[i].g;
				write_pixels[2] = source_pixel[i].b;
				write_pixels[3] = 255;
				write_pixels += 4;
			}

		}
	}
	CloseOSFile(image_file);
}

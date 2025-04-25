#include "BBMain.h"
#include "BBMemory.h"
#include "Program.h"
#include "BBThreadScheduler.hpp"

#include "BBImage.hpp"

#include "Storage/BBString.h"
#include <chrono>
#include <iostream>

#include "Utils.h"


using namespace BB;

constexpr float motion_blur_factor = 1.f / 9.f;
constexpr float motion_blur_bias = 0;
constexpr float motion_blur_filter[9 * 9]
{
	1, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 1, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 1, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 1, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 1, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 1, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 1, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 1, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 1,
};

constexpr float gaussian5x5_factor = 1.f / 256.f;
constexpr float gaussian5x5_bias = 0;
constexpr float gaussian5x5_filter[5 * 5]
{
	1, 4, 6, 4, 1,
	4, 16, 24, 16, 4,
	6, 24, 36, 24, 6,
	4, 16, 24, 16, 4,
	1, 4, 6, 4, 1
};

constexpr float blur5x5_factor = 1.f / 13.f;
constexpr float blur5x5_bias = 0;
constexpr float blur5x5_filter[5 * 5]
{
	0, 0, 1, 0, 0,
	0, 1, 1, 1, 0,
	1, 1, 1, 1, 1,
	0, 1, 1, 1, 0,
	0, 0, 1, 0, 0
};

constexpr float gaussian3x3_factor = 1.f / 16.f;
constexpr float gaussian3x3_bias = 0;
constexpr float gaussian3x3_filter[3 * 3]
{
	1.f, 2.f, 1.f,
	2.f, 4.f, 2.f,
	1.f, 2.f, 1.f,
};

#include "Math/Math.inl"

int main(int argc, char** argv)
{
	(void)argc;
	BBInitInfo bb_init;
	bb_init.exe_path = argv[0];
	bb_init.program_name = L"Modern Vulkan";
	InitBB(bb_init);

	SystemInfo sys_info;
	OSSystemInfo(sys_info);
	Threads::InitThreads(sys_info.processor_num);

	MemoryArena main_arena = MemoryArenaCreate();

	BBImage image{};
	image.Init(main_arena, "../resources/filter_textures/bmpimage.bmp");
	BBImage image_backup; 
	image_backup.Init(main_arena, image);
	image.WriteAsBMP("WriteNormal.bmp");
	image.WriteAsTARGA("WriteNormal.tga");

	using ms = std::chrono::duration<float, std::milli>;
	constexpr const float MILLITIMEDIVIDE = 1 / 1000.f;

	const uint32_t max_thread_count = sys_info.processor_num / 2;
	for (uint32_t thread_count = max_thread_count; thread_count > 0; --thread_count)
	{
		StackString<128> directory_names{"thread_count"};
		char thread_count_str[4]{};
		sprintf_s(thread_count_str, 3, "%u", thread_count);
		directory_names.append(thread_count_str);
		OSCreateDirectory(directory_names.c_str());
		directory_names.append("/");

		MemoryArenaScope(main_arena)
		{
			auto t_Timer = std::chrono::high_resolution_clock::now();
			image.FilterImage(main_arena, gaussian3x3_filter, 3, 3, gaussian3x3_factor, gaussian3x3_bias, thread_count);
			size_t cur_string_size = directory_names.size();
			directory_names.append("writegaussian3x3.bmp");
			image.WriteAsBMP(directory_names.c_str());
			directory_names.pop_back(static_cast<uint32_t>(directory_names.size() - cur_string_size));

			image = image_backup;
			image.FilterImage(main_arena, gaussian5x5_filter, 5, 5, gaussian5x5_factor, gaussian5x5_bias, thread_count);
			cur_string_size = directory_names.size();
			directory_names.append("writegaussian5x5.bmp");
			image.WriteAsBMP(directory_names.c_str());
			directory_names.pop_back(static_cast<uint32_t>(directory_names.size() - cur_string_size));

			image = image_backup;
			image.FilterImage(main_arena, blur5x5_filter, 5, 5, blur5x5_factor, blur5x5_bias, thread_count);
			cur_string_size = directory_names.size();
			directory_names.append("writeblur5x5.bmp");
			image.WriteAsBMP(directory_names.c_str());
			directory_names.pop_back(static_cast<uint32_t>(directory_names.size() - cur_string_size));

			image = image_backup;
			image.FilterImage(main_arena, motion_blur_filter, 9, 9, motion_blur_factor, motion_blur_bias, thread_count);
			cur_string_size = directory_names.size();
			directory_names.append("writemotion_blur.bmp");
			image.WriteAsBMP(directory_names.c_str());
			directory_names.pop_back(static_cast<uint32_t>(directory_names.size() - cur_string_size));

			image = image_backup;
			image.SharpenImage(main_arena, 1, thread_count);
			cur_string_size = directory_names.size();
			directory_names.append("writesharpen.bmp");
			image.WriteAsBMP(directory_names.c_str());
			directory_names.pop_back(static_cast<uint32_t>(directory_names.size() - cur_string_size));

			directory_names.pop_back(1);
			auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
			std::cout << directory_names.c_str() << " image processing " << t_Speed << "\n";
		}
	}

	MemoryArenaFree(main_arena);

	return 0;
}

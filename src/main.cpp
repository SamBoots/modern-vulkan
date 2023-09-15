#include "BBMain.h"
#include "BBMemory.h"
#include "Program.h"
#include "BBThreadScheduler.hpp"

#include "BBImage.hpp"


#include <chrono>
#include <iostream>


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

#include "Math.inl"

int main(int argc, char** argv)
{
	BBInitInfo bb_init;
	bb_init.exePath = argv[0];
	bb_init.programName = L"Modern Vulkan";
	InitBB(bb_init);
	
	Threads::InitThreads(16);

	StackAllocator_t allocator{ mbSize * 6 };
	
	{
		BBImage image1{ allocator, "resources/shapes.bmp" };
		BBImage image2{ allocator, image1 };
		BBImage image3{ allocator, image1 };
		BBImage image4{ allocator, image1 };
		image1.FilterImage(allocator, blur5x5_filter, 5, 5, blur5x5_factor, blur5x5_bias, 1);
		image2.FilterImage(allocator, blur5x5_filter, 5, 5, blur5x5_factor, blur5x5_bias, 2);
		image3.FilterImage(allocator, blur5x5_filter, 5, 5, blur5x5_factor, blur5x5_bias, 4);
		image4.FilterImage(allocator, blur5x5_filter, 5, 5, blur5x5_factor, blur5x5_bias, 8);
		image1.WriteAsBMP("1.bmp");
		image2.WriteAsBMP("2.bmp");
		image3.WriteAsBMP("4.bmp");
		image4.WriteAsBMP("8.bmp");
	}

	BBImage image{ allocator, "resources/shapes.bmp" };
	const BBImage image_backup{ allocator, image };
	image.WriteAsBMP("WriteNormal.bmp");

	typedef std::chrono::duration<float, std::milli> ms;
	constexpr const float MILLITIMEDIVIDE = 1 / 1000.f;

	{ //8 threads
		auto t_Timer = std::chrono::high_resolution_clock::now();
		constexpr uint32_t THREAD_COUNT = 8;
		uintptr_t stack_pos = allocator.GetPosition();
		image.FilterImage(allocator, gaussian3x3_filter, 3, 3, gaussian3x3_factor, gaussian3x3_bias, THREAD_COUNT);
		allocator.SetPosition(stack_pos);
		image.WriteAsBMP("thread8/writegaussian3x3.bmp");

		image = image_backup;
		stack_pos = allocator.GetPosition();
		image.FilterImage(allocator, gaussian5x5_filter, 5, 5, gaussian5x5_factor, gaussian5x5_bias, THREAD_COUNT);
		allocator.SetPosition(stack_pos);
		image.WriteAsBMP("thread8/writegaussian5x5.bmp");

		image = image_backup;
		stack_pos = allocator.GetPosition();
		image.FilterImage(allocator, blur5x5_filter, 5, 5, blur5x5_factor, blur5x5_bias, THREAD_COUNT);
		allocator.SetPosition(stack_pos);
		image.WriteAsBMP("thread8/writeblur5x5.bmp");

		image = image_backup;
		stack_pos = allocator.GetPosition();
		image.FilterImage(allocator, motion_blur_filter, 9, 9, motion_blur_factor, motion_blur_bias, THREAD_COUNT);
		allocator.SetPosition(stack_pos);
		image.WriteAsBMP("thread8/writemotion_blur.bmp");

		image = image_backup;
		stack_pos = allocator.GetPosition();
		image.SharpenImage(allocator, 1, THREAD_COUNT);
		allocator.SetPosition(stack_pos);
		image.WriteAsBMP("thread8/writesharpen.bmp");

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "8 threads image processing " << t_Speed << "\n";
	}

	{ //4 threads
		auto t_Timer = std::chrono::high_resolution_clock::now();
		constexpr uint32_t THREAD_COUNT = 4;
		uintptr_t stack_pos = allocator.GetPosition();
		image.FilterImage(allocator, gaussian3x3_filter, 3, 3, gaussian3x3_factor, gaussian3x3_bias, THREAD_COUNT);
		allocator.SetPosition(stack_pos);
		image.WriteAsBMP("thread4/writegaussian3x3.bmp");

		image = image_backup;
		stack_pos = allocator.GetPosition();
		image.FilterImage(allocator, gaussian5x5_filter, 5, 5, gaussian5x5_factor, gaussian5x5_bias, THREAD_COUNT);
		allocator.SetPosition(stack_pos);
		image.WriteAsBMP("thread4/writegaussian5x5.bmp");

		image = image_backup;
		stack_pos = allocator.GetPosition();
		image.FilterImage(allocator, blur5x5_filter, 5, 5, blur5x5_factor, blur5x5_bias, THREAD_COUNT);
		allocator.SetPosition(stack_pos);
		image.WriteAsBMP("thread4/writeblur5x5.bmp");

		image = image_backup;
		stack_pos = allocator.GetPosition();
		image.FilterImage(allocator, motion_blur_filter, 9, 9, motion_blur_factor, motion_blur_bias, THREAD_COUNT);
		allocator.SetPosition(stack_pos);
		image.WriteAsBMP("thread4/writemotion_blur.bmp");

		image = image_backup;
		stack_pos = allocator.GetPosition();
		image.SharpenImage(allocator, 1, THREAD_COUNT);
		allocator.SetPosition(stack_pos);
		image.WriteAsBMP("thread4/writesharpen.bmp");

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "4 threads image processing " << t_Speed << "\n";
	}

	{ //2 threads
		auto t_Timer = std::chrono::high_resolution_clock::now();
		constexpr uint32_t THREAD_COUNT = 2;
		uintptr_t stack_pos = allocator.GetPosition();
		image.FilterImage(allocator, gaussian3x3_filter, 3, 3, gaussian3x3_factor, gaussian3x3_bias, THREAD_COUNT);
		allocator.SetPosition(stack_pos);
		image.WriteAsBMP("thread2/writegaussian3x3.bmp");

		image = image_backup;
		stack_pos = allocator.GetPosition();
		image.FilterImage(allocator, gaussian5x5_filter, 5, 5, gaussian5x5_factor, gaussian5x5_bias, THREAD_COUNT);
		allocator.SetPosition(stack_pos);
		image.WriteAsBMP("thread2/writegaussian5x5.bmp");

		image = image_backup;
		stack_pos = allocator.GetPosition();
		image.FilterImage(allocator, blur5x5_filter, 5, 5, blur5x5_factor, blur5x5_bias, THREAD_COUNT);
		allocator.SetPosition(stack_pos);
		image.WriteAsBMP("thread2/writeblur5x5.bmp");

		image = image_backup;
		stack_pos = allocator.GetPosition();
		image.FilterImage(allocator, motion_blur_filter, 9, 9, motion_blur_factor, motion_blur_bias, THREAD_COUNT);
		allocator.SetPosition(stack_pos);
		image.WriteAsBMP("thread2/writemotion_blur.bmp");

		image = image_backup;
		stack_pos = allocator.GetPosition();
		image.SharpenImage(allocator, 1, THREAD_COUNT);
		allocator.SetPosition(stack_pos);
		image.WriteAsBMP("thread2/writesharpen.bmp");

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "2 threads image processing " << t_Speed << "\n";
	}

	{ //1 threads
		auto t_Timer = std::chrono::high_resolution_clock::now();
		constexpr uint32_t THREAD_COUNT = 1;
		uintptr_t stack_pos = allocator.GetPosition();
		image.FilterImage(allocator, gaussian3x3_filter, 3, 3, gaussian3x3_factor, gaussian3x3_bias, THREAD_COUNT);
		allocator.SetPosition(stack_pos);
		image.WriteAsBMP("single_thread/writegaussian3x3.bmp");

		image = image_backup;
		stack_pos = allocator.GetPosition();
		image.FilterImage(allocator, gaussian5x5_filter, 5, 5, gaussian5x5_factor, gaussian5x5_bias, THREAD_COUNT);
		allocator.SetPosition(stack_pos);
		image.WriteAsBMP("single_thread/writegaussian5x5.bmp");

		image = image_backup;
		stack_pos = allocator.GetPosition();
		image.FilterImage(allocator, blur5x5_filter, 5, 5, blur5x5_factor, blur5x5_bias, THREAD_COUNT);
		allocator.SetPosition(stack_pos);
		image.WriteAsBMP("single_thread/writeblur5x5.bmp");

		image = image_backup;
		stack_pos = allocator.GetPosition();
		image.FilterImage(allocator, motion_blur_filter, 9, 9, motion_blur_factor, motion_blur_bias, THREAD_COUNT);
		allocator.SetPosition(stack_pos);
		image.WriteAsBMP("single_thread/writemotion_blur.bmp");

		image = image_backup;
		stack_pos = allocator.GetPosition();
		image.SharpenImage(allocator, 1, THREAD_COUNT);
		allocator.SetPosition(stack_pos);
		image.WriteAsBMP("single_thread/writesharpen.bmp");

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "single thread image processing " << t_Speed << "\n";
	}
	{ //non SIMD
		auto t_Timer = std::chrono::high_resolution_clock::now();
		constexpr uint32_t THREAD_COUNT = 1;
		uintptr_t stack_pos = allocator.GetPosition();
		image.FilterImage(allocator, gaussian3x3_filter, 3, 3, gaussian3x3_factor, gaussian3x3_bias, THREAD_COUNT);
		allocator.SetPosition(stack_pos);
		image.WriteAsBMP("single_thread/writegaussian3x3.bmp");

		image = image_backup;
		stack_pos = allocator.GetPosition();
		image.FilterImage(allocator, gaussian5x5_filter, 5, 5, gaussian5x5_factor, gaussian5x5_bias, THREAD_COUNT);
		allocator.SetPosition(stack_pos);
		image.WriteAsBMP("single_thread/writegaussian5x5.bmp");

		image = image_backup;
		stack_pos = allocator.GetPosition();
		image.FilterImage(allocator, blur5x5_filter, 5, 5, blur5x5_factor, blur5x5_bias, THREAD_COUNT);
		allocator.SetPosition(stack_pos);
		image.WriteAsBMP("single_thread/writeblur5x5.bmp");

		image = image_backup;
		stack_pos = allocator.GetPosition();
		image.FilterImage(allocator, motion_blur_filter, 9, 9, motion_blur_factor, motion_blur_bias, THREAD_COUNT);
		allocator.SetPosition(stack_pos);
		image.WriteAsBMP("single_thread/writemotion_blur.bmp");

		image = image_backup;
		stack_pos = allocator.GetPosition();
		image.SharpenImage(allocator, 1, THREAD_COUNT);
		allocator.SetPosition(stack_pos);
		image.WriteAsBMP("single_thread/writesharpen.bmp");

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "single thread non-SIMD image processing " << t_Speed << "\n";
	}

	allocator.Clear();
	return 0;
}

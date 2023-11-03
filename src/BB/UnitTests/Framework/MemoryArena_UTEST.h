#pragma once
#include "../TestValues.h"
#include "BBMemory.h"

TEST(MemoryTesting, Create_Memory_Leak_and_tag)
{
	constexpr size_t allocatorSize = 1028;
	constexpr size_t allocationSize = 256;

	BB::LinearAllocator_t t_LinearAllocator(allocatorSize, "Leak tester");

	void* ptr = BBalloc(t_LinearAllocator, allocationSize);
	BB::BBTagAlloc(t_LinearAllocator, ptr, "memory leak tag");
	//Leak will accur.
}
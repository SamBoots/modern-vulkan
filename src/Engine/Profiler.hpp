#pragma once
#include "Enginefwd.hpp"
#include "Storage/BBString.h"
#include "Storage/Array.h"

namespace BB
{
	constexpr uint32_t PROFILE_RESULT_HISTORY_BUFFER_SIZE = 2048;
	struct ProfileResult
	{
		StackString<32> name;
		size_t profile_count;
		double start_time;
		double time_in_miliseconds;
		double average_time;
		double total_time;
		int line;
		const char* file;

		StaticArray<double> history;
	};

	void InitializeProfiler(MemoryArena& a_arena, const uint32_t a_max_profile_entries);
	// use BB_START_PROFILE instead of this function unless you know what you are doing
	void StartProfile_f(const int a_line, const char* a_file, const StackString<32>& a_name);
	// use BB_END_PROFILE instead of this function
	void EndProfile_f(const StackString<32>& a_name);

	ConstSlice<ProfileResult> GetProfileResultsList();

	#define BB_START_PROFILE(a_name) StartProfile_f(__LINE__, __FILE__, a_name)
	#define BB_END_PROFILE(a_name, a_write_to_file) EndProfile_f(a_name)
}

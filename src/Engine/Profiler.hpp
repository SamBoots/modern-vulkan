#pragma once
#include "Enginefwd.hpp"
#include "Storage/BBString.h"
#include "Storage/LinkedList.h"

namespace BB
{
	struct ProfileResults : public LinkedListNode<ProfileResults>
	{
		StackString<32> name;
		double start_time;
		double time_in_seconds;
		int line;
		const char* file;
	};

	void InitializeProfiler(MemoryArena& a_arena, const uint32_t a_max_profile_entries);
	// use BB_START_PROFILE instead of this function unless you know what you are doing
	void StartProfile_f(const int a_line, const char* a_file, const StackString<32>& a_name);
	// use BB_END_PROFILE instead of this function
	ProfileResults EndProfile_f(const StackString<32>& a_name, const bool a_write_to_file);

	LinkedList<ProfileResults> GetProfileResultsList();

	#define BB_START_PROFILE(a_name) StartProfile_f(__LINE__, __FILE__, a_name)
	#define BB_END_PROFILE(a_name, a_write_to_file) EndProfile_f(a_name, a_write_to_file)
}

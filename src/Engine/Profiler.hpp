#pragma once
#include "Enginefwd.hpp"
#include "Storage/BBString.h"
#include "Storage/LinkedList.h"

namespace BB
{
	struct ProfileResults : public LinkedListNode<ProfileResults>
	{
		uint64_t frame;
		StackString<32> name;
		int time_in_nano_seconds;
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
	#define BB_END_PROFILE(a_name, a_write_to_file) EndProfile_f(__LINE__, __FILE__, a_name, a_write_to_file)
}

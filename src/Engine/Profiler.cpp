#include "Profiler.hpp"
#include "Storage/Hashmap.h"
#include "Storage/Array.h"

#include "Program.h"
#include <chrono>

using namespace BB;

struct ProfilerSystem_inst
{
	StaticOL_HashMap<StackString<32>, ProfileResult*> profile_entries;
	uint32_t profile_count;
	StaticArray<ProfileResult> profile_results;
	BBRWLock lock;
};

static ProfilerSystem_inst* s_profiler;

static double GetTimeInnanoseconds()
{
	auto now = std::chrono::high_resolution_clock::now();
	auto duration = now.time_since_epoch();
	double nanoseconds = std::chrono::duration<double, std::milli>(duration).count();

	return nanoseconds;
}

void BB::InitializeProfiler(MemoryArena& a_arena, const uint32_t a_max_profile_entries)
{
	s_profiler = ArenaAllocType(a_arena, ProfilerSystem_inst);
	s_profiler->profile_entries.Init(a_arena, a_max_profile_entries);
	s_profiler->profile_results.Init(a_arena, a_max_profile_entries);
	s_profiler->profile_results.resize(a_max_profile_entries);
	for (uint32_t i = 0; i < a_max_profile_entries; i++)
	{
		s_profiler->profile_results[i].history.Init(a_arena, PROFILE_RESULT_HISTORY_BUFFER_SIZE);
	}
	s_profiler->lock = OSCreateRWLock();
	s_profiler->profile_count = 0;
}

void BB::StartProfile_f(const int a_line, const char* a_file, const StackString<32>& a_name)
{
	OSAcquireSRWLockWrite(&s_profiler->lock);

	if (!s_profiler->profile_entries.find(a_name))
	{
		ProfileResult* new_result = &s_profiler->profile_results[s_profiler->profile_count++];
		s_profiler->profile_entries.insert(a_name, new_result);

		new_result->name = a_name;
		new_result->line = a_line;
		new_result->file = a_file;
	}

	ProfileResult** presult = s_profiler->profile_entries.find(a_name);
	BB_ASSERT(presult != nullptr, "wrong hashtable insertion. This should not happen");

	ProfileResult* result = *presult;
	BB_ASSERT(result->start_time == 0, "started profile on an entry that is already recording!");

	result->start_time = GetTimeInnanoseconds();
	result->time_in_miliseconds = 0;

	OSReleaseSRWLockWrite(&s_profiler->lock);
}

// use BB_END_PROFILE instead of this function
void BB::EndProfile_f(const StackString<32>& a_name)
{
	ProfileResult** presult = s_profiler->profile_entries.find(a_name);
	if (!presult)
	{
		BB_WARNING(false, "did not start the profile results!", WarningType::MEDIUM);
		return;
	}
	ProfileResult* result = *presult;
	result->time_in_miliseconds = GetTimeInnanoseconds() - result->start_time;

	++result->profile_count;
	result->total_time += result->time_in_miliseconds;
	result->average_time = result->total_time / static_cast<double>(result->profile_count);
	result->start_time = 0;

	if (result->history.IsFull())
		result->history.clear();
	result->history.push_back(result->time_in_miliseconds);
}

ConstSlice<ProfileResult> BB::GetProfileResultsList()
{
	return s_profiler->profile_results.const_slice(s_profiler->profile_count);
}

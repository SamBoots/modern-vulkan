#include "Profiler.hpp"
#include "Storage/Hashmap.h"
#include "OS/Program.h"
#include <chrono>

using namespace BB;

struct ProfilerSystem_inst
{
	StaticOL_HashMap<StackString<32>, ProfileResults> profile_entries;
	LinkedList<ProfileResults> profile_entry_linked;
	BBRWLock lock;
};

static ProfilerSystem_inst* s_profiler;

void BB::InitializeProfiler(MemoryArena& a_arena, const uint32_t a_max_profile_entries)
{
	s_profiler = ArenaAllocType(a_arena, ProfilerSystem_inst);
	s_profiler->profile_entries.Init(a_arena, a_max_profile_entries);
	s_profiler->lock = OSCreateRWLock();
}

void BB::StartProfile_f(const int a_line, const char* a_file, const StackString<32>& a_name)
{
	OSAcquireSRWLockWrite(&s_profiler->lock);

	ProfileResults* results = s_profiler->profile_entries.find(a_name);
	if (!results)
	{
		s_profiler->profile_entries.insert(a_name, ProfileResults());
		results = s_profiler->profile_entries.find(a_name);
		BB_ASSERT(results, "incorrectly inserted element in profile entries, this should not happen");
		results->name = a_name;
		results->line = a_line;
		results->file = a_file;
		s_profiler->profile_entry_linked.Push(results);
	}

	auto current_time = std::chrono::system_clock::now();
	auto duration_in_seconds = std::chrono::duration<double>(current_time.time_since_epoch());
	results->start_time = std::chrono::duration<double, std::nano>(duration_in_seconds.count()).count();
	results->time_in_seconds = 0;

	OSReleaseSRWLockWrite(&s_profiler->lock);
}

// use BB_END_PROFILE instead of this function
ProfileResults BB::EndProfile_f(const StackString<32>& a_name, const bool a_write_to_file)
{
	ProfileResults* results = s_profiler->profile_entries.find(a_name);
	if (!results)
	{
		BB_WARNING(false, "did not start the profile results!", WarningType::MEDIUM);
		return ProfileResults();
	}

	auto current_time = std::chrono::system_clock::now();
	auto duration_in_seconds = std::chrono::duration<double>(current_time.time_since_epoch());
	double end_time = std::chrono::duration<double, std::nano>(duration_in_seconds.count()).count();
	results->time_in_seconds = end_time - results->start_time;

	return *results;
}

LinkedList<ProfileResults> BB::GetProfileResultsList()
{
	return s_profiler->profile_entry_linked;
}

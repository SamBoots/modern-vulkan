#include "Profiler.hpp"
#include "Storage/Hashmap.h"
#include "Storage/Array.h"

#include "Program.h"
#include <chrono>

using namespace BB;

static void CountingRingBufferPush(CountingRingBuffer& a_buff, const double a_value)
{
	a_buff.sum -= a_buff.buffer[a_buff.head];
	a_buff.sum += a_value;
	a_buff.buffer[a_buff.head] = a_value;

	// we waste 1 slot but who cares
	if (((a_buff.head + 1) % a_buff.max_size) == a_buff.tail)
		a_buff.tail = (a_buff.tail + 1) % a_buff.max_size;

	a_buff.head = (a_buff.head + 1) % a_buff.max_size;
}

static uint32_t CountingRingBufferSize(const CountingRingBuffer& a_buff)
{
	if (a_buff.head != a_buff.tail)
	{
		if (a_buff.head >= a_buff.tail)
		{
			return a_buff.head - a_buff.tail;
		}
		else
		{
			return a_buff.max_size + a_buff.head - a_buff.tail;
		}
	}

	return a_buff.max_size;
}

StaticArray<double> BB::CountingRingBufferLinear(MemoryArena& a_arena, const CountingRingBuffer& a_buff)
{
	StaticArray<double> arr{};
	arr.Init(a_arena, a_buff.max_size);
	
	const size_t copy_size = CountingRingBufferSize(a_buff);
	if (copy_size + a_buff.tail >= a_buff.max_size)
	{
		const size_t remainder = (copy_size + a_buff.tail) % a_buff.max_size;

		arr.push_back(&a_buff.buffer[a_buff.tail], copy_size - remainder);
		arr.push_back(&a_buff.buffer[0], remainder);
	}
	else
	{
		arr.push_back(&a_buff.buffer[a_buff.tail], copy_size);
	}

	return arr;
}

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
		s_profiler->profile_results[i].history_buffer.buffer = ArenaAllocArr(a_arena, double, PROFILE_RESULT_HISTORY_BUFFER_SIZE);
		s_profiler->profile_results[i].history_buffer.max_size = PROFILE_RESULT_HISTORY_BUFFER_SIZE;
		s_profiler->profile_results[i].history_buffer.head = 0;
		s_profiler->profile_results[i].history_buffer.tail = 0;
		s_profiler->profile_results[i].history_buffer.sum = 0;
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
	BB_ASSERT(DoubleEqualNoWarning(result->start_time, 0), "started profile on an entry that is already recording!");

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

	CountingRingBufferPush(result->history_buffer, result->time_in_miliseconds);
	result->average_time = result->history_buffer.sum / static_cast<double>(CountingRingBufferSize(result->history_buffer));
	result->start_time = 0;
}

ConstSlice<ProfileResult> BB::GetProfileResultsList()
{
	return s_profiler->profile_results.const_slice(s_profiler->profile_count);
}

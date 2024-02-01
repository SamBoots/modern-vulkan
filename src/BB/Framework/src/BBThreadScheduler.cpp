#include "BBThreadScheduler.hpp"
#include "Program.h"

using namespace BB;

enum class THREAD_STATUS : uint32_t
{
	IDLE,
	BUSY,
	DESTROY
};

struct ThreadInfo
{
	void(*function)(void*);
	void* function_parameter;
	THREAD_STATUS thread_status;
	//debug value for the ThreadHandle extra_index;
	BBConditionalVariable condition;
	const wchar_t* task_name;
	uint32_t generation;
};

#pragma optimize( "", off ) 
static void ThreadStartFunc(void* a_args)
{
	ThreadInfo* thread_info = reinterpret_cast<ThreadInfo*>(a_args);
	BBRWLock lock = OSCreateRWLock();
	OSAcquireSRWLockWrite(&lock);

	while (thread_info->thread_status != THREAD_STATUS::DESTROY)
	{
		OSWaitConditionalVariableExclusive(&thread_info->condition, &lock);
		OSSetThreadName(thread_info->task_name);
		thread_info->function(thread_info->function_parameter);
		++thread_info->generation;
		thread_info->function = nullptr;
		thread_info->function_parameter = nullptr;
		thread_info->thread_status = THREAD_STATUS::IDLE;
		OSSetThreadName(L"none");
	}
}
#pragma optimize( "", on )

struct Thread
{
	OSThreadHandle os_thread_handle{};
	ThreadInfo thread_info; //used to send functions to threads
};

struct ThreadScheduler
{
	uint32_t thread_count = 0;
	Thread threads[32]{};
};

static ThreadScheduler s_ThreadScheduler{};

BB::Threads::Barrier::Barrier(const uint32_t a_thread_count)
	: thread_count(a_thread_count)
{
	count = 0;
	barrier = OSCreateSemaphore(0, 1); //initially locked
}

BB::Threads::Barrier::~Barrier()
{
	OSDestroySemaphore(barrier);
}

void BB::Threads::Barrier::Wait()
{
	OSWaitSemaphore(barrier);
}

void BB::Threads::Barrier::Signal()
{
	if (++count >= thread_count)
		OSSignalSemaphore(barrier, 1);
}


void BB::Threads::InitThreads(const uint32_t a_thread_count)
{
	BB_ASSERT(a_thread_count < _countof(s_ThreadScheduler.threads), "Trying to create too many threads!");
	s_ThreadScheduler.thread_count = a_thread_count;

	for (uint32_t i = 0; i < s_ThreadScheduler.thread_count; i++)
	{
		s_ThreadScheduler.threads[i].thread_info.function = nullptr;
		s_ThreadScheduler.threads[i].thread_info.function_parameter = nullptr;
		s_ThreadScheduler.threads[i].thread_info.thread_status = THREAD_STATUS::IDLE;
		s_ThreadScheduler.threads[i].thread_info.generation = 0;
		s_ThreadScheduler.threads[i].thread_info.condition = OSCreateConditionalVariable();
		s_ThreadScheduler.threads[i].os_thread_handle = OSCreateThread(ThreadStartFunc,
			0,
			&s_ThreadScheduler.threads[i].thread_info);
	}
}

void BB::Threads::DestroyThreads()
{
	for (uint32_t i = 0; i < s_ThreadScheduler.thread_count; i++)
	{
		s_ThreadScheduler.threads[i].thread_info.thread_status = THREAD_STATUS::DESTROY;
	}
}

ThreadTask BB::Threads::StartTaskThread(void(*a_Function)(void*), void* a_FuncParameter, const wchar_t* a_task_name)
{
	for (uint32_t i = 0; i < s_ThreadScheduler.thread_count; i++)
	{
		if (s_ThreadScheduler.threads[i].thread_info.thread_status == THREAD_STATUS::IDLE)
		{
			s_ThreadScheduler.threads[i].thread_info.task_name = a_task_name;
			s_ThreadScheduler.threads[i].thread_info.function = a_Function;
			s_ThreadScheduler.threads[i].thread_info.function_parameter = a_FuncParameter;
			s_ThreadScheduler.threads[i].thread_info.thread_status = THREAD_STATUS::BUSY;
			OSWakeConditionVariable(&s_ThreadScheduler.threads[i].thread_info.condition);
			return ThreadTask(i, s_ThreadScheduler.threads[i].thread_info.generation + 1);
		}
	}
	BB_ASSERT(false, "No free threads! Maybe implement a way to just re-iterate over the list again.");
	return ThreadTask(BB_INVALID_HANDLE_64);
}

void BB::Threads::WaitForTask(const ThreadTask a_handle)
{
	while (s_ThreadScheduler.threads[a_handle.index].thread_info.generation < a_handle.extra_index) {}
}

bool BB::Threads::TaskFinished(const ThreadTask a_handle)
{
	if (s_ThreadScheduler.threads[a_handle.index].thread_info.generation <= a_handle.extra_index)
		return true;

	return false;
}

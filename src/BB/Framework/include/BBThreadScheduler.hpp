#pragma once
#include "Common.h"
#include <atomic>

namespace BB
{
	namespace Threads
	{
		class Barrier
		{
		public:
			Barrier(const uint32_t a_thread_count);
			~Barrier();

			void Wait();
			void Signal();
			
		private:
			const uint32_t thread_count;
			std::atomic_uint32_t count;
			BBSemaphore barrier;
		};

		void InitThreads(const uint32_t a_thread_count);
		void DestroyThreads();
		ThreadTask StartTaskThread(void(*a_Function)(void*), void* a_FuncParameter);

		void WaitForTask(const ThreadTask a_handle);
		bool TaskFinished(const ThreadTask a_handle);
	}
}

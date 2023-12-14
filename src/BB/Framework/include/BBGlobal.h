#pragma once
#include "Allocators/MemoryArena.hpp"
#include "Storage/BBString.h"

namespace BB
{
	extern const wchar* g_program_name;
	extern const char* g_exe_path;

	struct Logger_inst
	{
		MemoryArena memory_arena;

		uint32_t max_logger_buffer_size;
		WarningTypeFlags enabled_warning_flags;
		String cache_string;
		String upload_string;
		ThreadTask last_thread_task = ThreadTask(BB_INVALID_HANDLE_64);
		BBMutex write_to_file_mutex;
		OSFileHandle log_file;
	};
	extern Logger_inst g_logger;

#ifdef _DEBUG
	extern OSFileHandle g_AllocationLogFile;
#endif //_DEBUG
}

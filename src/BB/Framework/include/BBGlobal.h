#pragma once
#include "Allocators/MemoryArena.hpp"
#include "Storage/BBString.h"

namespace BB
{
	inline const wchar* g_program_name;
	inline const char* g_exe_path;

	struct Logger_inst
	{
		MemoryArena memory_arena;

		uint32_t max_logger_buffer_size;
		WarningTypeFlags enabled_warning_flags;
		String cache_string;
		String upload_string;
		ThreadTask last_thread_task;
		BBMutex write_to_file_mutex;
		OSFileHandle log_file;
	};
	inline Logger_inst* g_logger;

#ifdef _DEBUG
	inline OSFileHandle g_AllocationLogFile;
#endif //_DEBUG
}

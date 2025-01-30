#pragma once
#include "Allocators/MemoryArena.hpp"
#include "Storage/BBString.h"

namespace BB
{
	inline const wchar* g_program_name;
	inline const char* g_exe_path;

#ifdef _DEBUG
	inline OSFileHandle g_AllocationLogFile;
#endif //_DEBUG
}

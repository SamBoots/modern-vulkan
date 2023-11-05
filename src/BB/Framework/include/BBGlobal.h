#pragma once
#include "Common.h"

namespace BB
{
	extern const wchar* g_program_name;
	extern const char* g_ExePath;

#ifdef _DEBUG
	extern OSFileHandle g_AllocationLogFile;
#endif //_DEBUG
}

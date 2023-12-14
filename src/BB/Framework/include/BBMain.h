#pragma once
#include "Common.h"

namespace BB
{
	constexpr uint32_t DEFAULT_LOGGER_BUFFER_STORAGE_SIZE = 2048;

	struct BBInitInfo
	{
		const wchar* program_name;
		const char* exe_path;

		const uint32_t logger_buffer_storage = DEFAULT_LOGGER_BUFFER_STORAGE_SIZE;
		const WarningTypeFlags logger_enabled_warning_flags = WARNING_TYPES_ALL;
	};

	void InitBB(const BBInitInfo& a_BBInfo);
	void DestroyBB();

	const wchar* GetProgramName();
	const char* GetProgramPath();
}

#include "BBGlobal.h"
#include "BBMain.h"
#include "Program.h"

using namespace BB;

const wchar* BB::g_program_name;
const char* BB::g_exe_path;
Logger_inst BB::g_logger;

#ifdef _DEBUG
OSFileHandle BB::g_AllocationLogFile;
#endif //_DEBUG

void BB::InitBB(const BBInitInfo& a_bb_info)
{
	g_program_name = a_bb_info.program_name;
	g_exe_path = a_bb_info.exe_path;

#ifdef _DEBUG
	g_AllocationLogFile = CreateOSFile(L"allocationLogger.txt");
#endif //_DEBUG

	g_logger.memory_arena = MemoryArenaCreate();
	g_logger.max_logger_buffer_size = a_bb_info.logger_buffer_storage;
	g_logger.cache_string = String(g_logger.memory_arena, a_bb_info.logger_buffer_storage);
	g_logger.upload_string = String(g_logger.memory_arena, a_bb_info.logger_buffer_storage);
	g_logger.write_to_file_mutex = OSCreateMutex();
	g_logger.log_file = CreateOSFile(L"logger.txt");

	g_logger.enabled_warning_flags = a_bb_info.logger_enabled_warning_flags;

	if (!OSSetCurrentDirectory(g_exe_path))
		LatestOSError();

	InitProgram();
}

void BB::DestroyBB()
{
	Logger::LoggerWriteToFile();

	MemoryArenaFree(g_logger.memory_arena);
	OSDestroyMutex(g_logger.write_to_file_mutex);
}

const wchar* BB::GetProgramName()
{
	return g_program_name;
}

const char* BB::GetProgramPath()
{
	return g_exe_path;
}

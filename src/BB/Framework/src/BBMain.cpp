#include "BBGlobal.h"
#include "BBMain.h"
#include "Program.h"

using namespace BB;

void BB::InitBB(const BBInitInfo& a_bb_info)
{
	g_program_name = a_bb_info.program_name;
	g_exe_path = a_bb_info.exe_path;

#ifdef _DEBUG
	g_AllocationLogFile = OSCreateFile(L"allocationLogger.txt");
#endif //_DEBUG

	{
		MemoryArena arena = MemoryArenaCreate();
		g_logger = ArenaAllocType(arena, Logger_inst);
		g_logger->memory_arena = MemoryArenaCreate();
	}
	g_logger->max_logger_buffer_size = a_bb_info.logger_buffer_storage;
	g_logger->cache_string = String(g_logger->memory_arena, a_bb_info.logger_buffer_storage);
	g_logger->upload_string = String(g_logger->memory_arena, a_bb_info.logger_buffer_storage);
	g_logger->write_to_file_mutex = OSCreateMutex();
	g_logger->log_file = OSCreateFile(L"logger.txt");

	g_logger->enabled_warning_flags = a_bb_info.logger_enabled_warning_flags;

	if (!OSSetCurrentDirectory(g_exe_path))
		LatestOSError();

	InitProgram();
}

void BB::DestroyBB()
{
	Logger::LoggerWriteToFile();

	OSDestroyMutex(g_logger->write_to_file_mutex);
	CloseOSFile(g_logger->log_file);
	MemoryArenaFree(g_logger->memory_arena);

}

const wchar* BB::GetProgramName()
{
	return g_program_name;
}

const char* BB::GetProgramPath()
{
	return g_exe_path;
}

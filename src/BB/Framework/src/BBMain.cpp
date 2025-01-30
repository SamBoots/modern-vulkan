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

	if (!OSSetCurrentDirectory(g_exe_path))
		LatestOSError();

	InitProgram();
}

void BB::DestroyBB()
{

}

const wchar* BB::GetProgramName()
{
	return g_program_name;
}

const char* BB::GetProgramPath()
{
	return g_exe_path;
}

////// PROGRAM.h //////
/// Program abstracts most OS calls and handles creation of windows,
/// loading of external files and book keeping of data that is part of
/// the program. Such as the exe location and name.
/// 
/// Replaces OSDevice.h from the older BB version
////// PROGRAM.h //////

#pragma once
#include <cstdlib>
#include <cstdint>
#include "Common.h"
#include "BBMemory.h"
#include "Storage/BBString.h"

namespace BB
{
	using LibFuncPtr = void*;
	typedef void (*PFN_WindowResizeEvent)(const WindowHandle a_window_handle, const uint32_t a_X, const uint32_t a_Y);
	typedef void (*PFN_WindowCloseEvent)(const WindowHandle a_window_handle);

	using OSThreadHandle = FrameworkHandle<struct ThreadHandletag>;

	enum class OS_WINDOW_STYLE
	{
		MAIN, //This window has a menu bar.
		CHILD //This window does not have a menu bar.
	};

	//This should match the interger defines for the specific OS
	enum class OS_FILE_READ_POINT : uint32_t
	{
#ifdef WIN32
		BEGIN = 0,
		CURRENT = 1,
		END = 2
#elif _LINUX //Need to test for linux
		//BEGIN = 0,
		//CURRENT = 1,
		//END = 2
#endif
	};

	//Hide this in the future so that users cannot access it.
	void InitProgram();

	struct SystemInfo
	{
		uint32_t processor_num;
		uint32_t page_size;
		uint32_t allocation_granularity;
	};

	void OSSystemInfo(SystemInfo& a_system_info);
	uint32_t OSPageSize();

	void* ReserveVirtualMemory(const size_t a_size);
	bool CommitVirtualMemory(void* a_ptr, const size_t a_size);
	bool ReleaseVirtualMemory(void* a_ptr);

	//Prints the latest OS error and returns the error code, if it has no error code it returns 0.
	uint32_t LatestOSError();

	//Load a dynamic library
	LibHandle LoadLib(const wchar* a_lib_name);
	//Unload a dynamic library
	void UnloadLib(const LibHandle a_handle);
	//Load dynamic library function
	LibFuncPtr LibLoadFunc(const LibHandle a_handle, const char* a_func_name);

	//Write to the standard C++ console if it's available.
	void WriteToConsole(const char* a_string, uint32_t a_str_length);
	//Write to the standard C++ console if it's available.
	void WriteToConsole(const wchar_t* a_string, uint32_t a_str_length);

	void OSCreateDirectory(const char* a_path_name);
	
	bool OSFileIsValid(const OSFileHandle a_file_handle);
	//char replaced with string view later on.
	//handle is 0 if it failed to create the file, it will assert on failure.
	OSFileHandle CreateOSFile(const char* a_file_name);
	OSFileHandle CreateOSFile(const wchar* a_file_name);
	//char replaced with string view later on.
	//handle is 0 if it failed to load the file.
	OSFileHandle LoadOSFile(const char* a_file_name);
	OSFileHandle LoadOSFile(const wchar* a_file_name);
	//char replaced with string view later on.
	void WriteToOSFile(const OSFileHandle a_file_handle, const void* a_data, const size_t a_size);
	//Reads a loaded file.
	//Buffer.data will have a dynamic allocation from the given allocator.
	Buffer ReadOSFile(Allocator a_system_allocator, const OSFileHandle a_file_handle);
	//Reads an external file from path.
	//Buffer.data will have a dynamic allocation from the given allocator.
	Buffer ReadOSFile(Allocator a_system_allocator, const char* a_path);
	Buffer ReadOSFile(Allocator a_system_allocator, const wchar* a_path);
	//Get a file's size in bytes.
	uint64_t GetOSFileSize(const OSFileHandle a_file_handle);
	//Set the file position, a_offset can be 0 if you just want to move it to BEGIN or END.
	void SetOSFilePosition(const OSFileHandle a_file_handle, const uint32_t a_offset, const OS_FILE_READ_POINT a_file_read_point);

	void CloseOSFile(const OSFileHandle a_file_handle);

	OSThreadHandle OSCreateThread(void(*a_func)(void*), const unsigned int a_stack_size, void* a_arg_list);
	void OSWaitThreadfinish(const OSThreadHandle a_thread);

	BBMutex OSCreateMutex();
	void OSWaitAndLockMutex(const BBMutex a_mutex);
	void OSUnlockMutex(const BBMutex a_mutex);
	void OSDestroyMutex(const BBMutex a_mutex);

	BBSemaphore OSCreateSemaphore(const uint32_t a_initial_count, const uint32_t a_maximum_count);
	void OSWaitSemaphore(const BBSemaphore a_semaphore);
	void OSSignalSemaphore(const BBSemaphore a_semaphore, const uint32_t a_signal_count);
	void OSDestroySemaphore(const BBSemaphore a_Semaphore);

	BBRWLock OSCreateRWLock();
	void OSAcquireSRWLockRead(BBRWLock* a_lock);
	void OSAcquireSRWLockWrite(BBRWLock* a_lock);
	void OSReleaseSRWLockRead(BBRWLock* a_lock);
	void OSReleaseSRWLockWrite(BBRWLock* a_lock);

	BBConditionalVariable OSCreateConditionalVariable();
	void OSWaitConditionalVariableShared(BBConditionalVariable* a_condition, BBRWLock* a_lock);
	void OSWaitConditionalVariableExclusive(BBConditionalVariable* a_condition, BBRWLock* a_lock);
	void OSWakeConditionVariable(BBConditionalVariable* a_condition);

	WindowHandle CreateOSWindow(const OS_WINDOW_STYLE a_style, const int a_X, const int a_Y, const int a_width, const int a_height, const wchar* a_window_name);
	//Get the OS window handle (hwnd for windows as en example. Reinterpret_cast the void* to the hwnd).
	void* GetOSWindowHandle(const WindowHandle a_handle);
	void GetWindowSize(const WindowHandle a_handle, int& a_X, int& a_Y);
	void DirectDestroyOSWindow(const WindowHandle a_handle);
	void FreezeMouseOnWindow(const WindowHandle a_handle);
	void UnfreezeMouseOnWindow();

	//The function that will be called when a window is closed.
	void SetCloseWindowPtr(PFN_WindowCloseEvent a_func);
	//The function that will be called when a window is resized.
	void SetResizeEventPtr(PFN_WindowResizeEvent a_func);

	//Exits the application.
	void ExitApp();

	//Process the OS (or window) messages
	bool ProcessMessages(const WindowHandle a_window_handle);

	//Get the program name.
	const wchar* ProgramName();
	//Get the path where the project's exe file is located.
	const char* ProgramPath();
}

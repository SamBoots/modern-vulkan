#include "BBGlobal.h"
#include "Program.h"
#include "HID.inl"
#include "Math.inl"
#include "Utils/Logger.h"
#include "RingAllocator.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <fileapi.h>
#include <memoryapi.h>
#include <libloaderapi.h>
#include <WinUser.h>
#include <hidusage.h>
#include <commdlg.h>
#include <ShlObj_core.h>

#include <mutex>

using namespace BB;

static void DefaultClose(WindowHandle) {}
static void DefaultResize(WindowHandle, uint32_t, uint32_t) {}

static PFN_WindowCloseEvent s_pfn_close_event = DefaultClose;
static PFN_WindowResizeEvent s_pfn_resize_event = DefaultResize;

struct InputBuffer
{
	InputEvent input_buffer[INPUT_EVENT_BUFFER_MAX];
	uint32_t start = 0;
	uint16_t pos = 0;
	uint16_t used = 0;
};

struct GlobalProgramInfo
{
	bool tracking_mouse = true;
};

static BBRWLock s_input_lock{ OSCreateRWLock() };
static GlobalProgramInfo s_program_info;
static InputBuffer s_input_buffer{};

static void PushInput(const InputEvent& a_Input)
{
	OSAcquireSRWLockWrite(&s_input_lock);
	if (s_input_buffer.pos + 1 > INPUT_EVENT_BUFFER_MAX)
		s_input_buffer.pos = 0;

	s_input_buffer.input_buffer[s_input_buffer.pos++] = a_Input;

	//Since when we get the input we get all of it. 
	if (s_input_buffer.used < INPUT_EVENT_BUFFER_MAX)
	{
		++s_input_buffer.used;
	}
	OSReleaseSRWLockWrite(&s_input_lock);
}

//Returns false if no input is left.
static void GetAllInput(InputEvent* a_InputBuffer)
{
	OSAcquireSRWLockWrite(&s_input_lock);
	size_t first_index = s_input_buffer.start;
	for (size_t i = 0; i < s_input_buffer.used; i++)
	{
		a_InputBuffer[i] = s_input_buffer.input_buffer[first_index];
		//We go back to zero the read the data.
		if (++first_index > INPUT_EVENT_BUFFER_MAX)
			first_index = 0;
	}
	s_input_buffer.start = s_input_buffer.pos;
	s_input_buffer.used = 0;
	OSReleaseSRWLockWrite(&s_input_lock);
}

void BB::InitProgram()
{
	SetupHIDTranslates();
}

// Custom callback for the Windows proc.
// with assistance from https://handmade.network/forums/t/2011-keyboard_inputs_-_scancodes%252C_raw_input%252C_text_input%252C_key_names#10467
// to help with handling the unicode
static LRESULT wm_input(HWND a_hwnd, WPARAM a_wparam, LPARAM a_lparam)
{
	HRAWINPUT h_raw_input = reinterpret_cast<HRAWINPUT>(a_lparam);

	//Allocate an input event.
	InputEvent event{};

	UINT size = sizeof(RAWINPUT);
	RAWINPUT input{};
	GetRawInputData(h_raw_input, RID_INPUT, &input, &size, sizeof(RAWINPUTHEADER));

	if (input.header.dwType == RIM_TYPEKEYBOARD)
	{
		const RAWKEYBOARD& rk = input.data.keyboard;
		event.input_type = INPUT_TYPE::KEYBOARD;
		uint16_t scan_code = rk.MakeCode;

		static unsigned char KEY_STATES[256] = {};

		bool e0 = rk.Flags & RI_KEY_E0;
		bool e1 = rk.Flags & RI_KEY_E1;
		(void)e1; // maybe used later

#define UPDATE_KEY_STATE(key) do{KEY_STATES[key] = (rk.Flags & 1) ? 0 : 0xff;}while(0)

		switch (rk.VKey)
		{

		case VK_CONTROL:
			if (e0)
				UPDATE_KEY_STATE(VK_RCONTROL);
			else
				UPDATE_KEY_STATE(VK_LCONTROL);
			KEY_STATES[VK_CONTROL] = KEY_STATES[VK_RCONTROL] | KEY_STATES[VK_LCONTROL];
			break;

		case VK_SHIFT:
			UPDATE_KEY_STATE(MapVirtualKey(rk.MakeCode, MAPVK_VSC_TO_VK_EX));
			KEY_STATES[VK_SHIFT] = KEY_STATES[VK_LSHIFT] | KEY_STATES[VK_RSHIFT];
			break;
		case VK_MENU:
			if (e0)
				UPDATE_KEY_STATE(VK_LMENU);
			else
				UPDATE_KEY_STATE(VK_RMENU);
			KEY_STATES[VK_MENU] = KEY_STATES[VK_RMENU] | KEY_STATES[VK_LMENU];
			break;
		default:
			UPDATE_KEY_STATE(rk.VKey);
			break;
		}

		wchar unicode[16] = {};
		if (!(rk.Flags & 0x1)) //down
		{
			ToUnicode(input.data.keyboard.VKey, scan_code, KEY_STATES, unicode, _countof(unicode), 0);
		}

		event.key_info.scan_code = s_translate_key[scan_code];
		event.key_info.key_pressed = !(input.data.keyboard.Flags & RI_KEY_BREAK);
		event.key_info.utf16 = unicode[0];

		PushInput(event);
	}
	else if (input.header.dwType == RIM_TYPEMOUSE && s_program_info.tracking_mouse)
	{
		event.input_type = INPUT_TYPE::MOUSE;
		const float2 move_input{ 
			static_cast<float>(input.data.mouse.lLastX), 
			static_cast<float>(input.data.mouse.lLastY) };
		if (input.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE)
		{
			BB_ASSERT(false, "Windows Input, not using MOUSE_MOVE_ABSOLUTE currently.");
			//event.mouse_info.move_offset = move_input - s_InputInfo.mouse.oldPos;
			//s_InputInfo.mouse.oldPos = move_input;
		}
		else
		{
			event.mouse_info.move_offset = move_input;
			POINT point;
			BB_ASSERT(GetCursorPos(&point), "failed to get cursor pos");
			ScreenToClient(a_hwnd, &point);
			event.mouse_info.mouse_pos = { static_cast<float>(point.x), static_cast<float>(point.y) };
		}

		event.mouse_info.left_pressed = input.data.mouse.usButtonFlags & RI_MOUSE_BUTTON_1_DOWN;
		event.mouse_info.left_released = input.data.mouse.usButtonFlags & RI_MOUSE_BUTTON_1_UP;
		event.mouse_info.right_pressed = input.data.mouse.usButtonFlags & RI_MOUSE_BUTTON_2_DOWN;
		event.mouse_info.right_released = input.data.mouse.usButtonFlags & RI_MOUSE_BUTTON_2_UP;
		event.mouse_info.middle_pressed = input.data.mouse.usButtonFlags & RI_MOUSE_BUTTON_3_DOWN;
		event.mouse_info.middle_released = input.data.mouse.usButtonFlags & RI_MOUSE_BUTTON_3_UP;
		if (input.data.mouse.usButtonFlags & (RI_MOUSE_WHEEL | WM_MOUSEHWHEEL))
		{
			const int16_t mouse_move = *reinterpret_cast<const int16_t*>(&input.data.mouse.usButtonData);
			event.mouse_info.wheel_move = mouse_move / WHEEL_DELTA;
		}
		PushInput(event);
	}


	return DefWindowProcW(a_hwnd, WM_INPUT, a_wparam, a_lparam);
}

//Custom callback for the Windows proc.
static LRESULT CALLBACK WindowProc(HWND a_hwnd, UINT a_msg, WPARAM a_wparam, LPARAM a_lparam)
{

	switch (a_msg)
	{
	case WM_QUIT:
		break;
	case WM_DESTROY:
		s_pfn_close_event(WindowHandle(reinterpret_cast<uint64_t>(a_hwnd)));
		break;
	case WM_SIZE:
	{
		uint32_t x = static_cast<uint32_t>(LOWORD(a_lparam));
		uint32_t y = static_cast<uint32_t>(HIWORD(a_lparam));
		s_pfn_resize_event(WindowHandle(reinterpret_cast<uint64_t>(a_hwnd)), x, y);
		break;
	}
	case WM_MOUSELEAVE:
		s_program_info.tracking_mouse = false;
		break;
	case WM_MOUSEMOVE:
		s_program_info.tracking_mouse = true;
		break;
	case WM_INPUT:
		return wm_input(a_hwnd, a_wparam, a_lparam);
	}

	return DefWindowProcW(a_hwnd, a_msg, a_wparam, a_lparam);
}

void BB::OSSystemInfo(SystemInfo& a_system_info)
{
	SYSTEM_INFO sys_info;
	GetSystemInfo(&sys_info);

	a_system_info.processor_num = sys_info.dwNumberOfProcessors;
	a_system_info.page_size = sys_info.dwPageSize;
	a_system_info.allocation_granularity = sys_info.dwAllocationGranularity;
}

uint32_t BB::OSPageSize()
{
	SYSTEM_INFO sys_info;
	GetSystemInfo(&sys_info);
	return sys_info.dwPageSize;
}

void* BB::ReserveVirtualMemory(const size_t a_size)
{
	return VirtualAlloc(nullptr, a_size, MEM_RESERVE, PAGE_NOACCESS);
}

bool BB::CommitVirtualMemory(void* a_ptr, const size_t a_size)
{
	void* ptr = VirtualAlloc(a_ptr, a_size, MEM_COMMIT, PAGE_READWRITE);
	return ptr;
}

bool BB::DecommitVirtualMemory(void* a_ptr, const size_t a_size)
{
	return VirtualFree(a_ptr, a_size, MEM_DECOMMIT);
}

bool BB::ReleaseVirtualMemory(void* a_ptr)
{
	return VirtualFree(a_ptr, 0, MEM_RELEASE);
}

uint32_t BB::LatestOSError()
{
	DWORD error_msg = GetLastError();
	if (error_msg == 0)
		return 0;
	LPSTR message = nullptr;
	
	FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr,
		error_msg,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
		reinterpret_cast<LPSTR>(&message),
		0, nullptr);

	if (message == nullptr)
		LatestOSError();

	BB_WARNING(false, message, WarningType::HIGH);

	LocalFree(message);

	return static_cast<uint32_t>(error_msg);
}

LibHandle BB::LoadLib(const wchar* a_lib_name)
{
	return LibHandle(reinterpret_cast<uintptr_t>(LoadLibraryW(a_lib_name)));
}

bool BB::UnloadLib(const LibHandle a_handle)
{
	return FreeLibrary(reinterpret_cast<HMODULE>(a_handle.handle));
}

LibFuncPtr BB::LibLoadFunc(const LibHandle a_handle, const char* a_func_name)
{
	LibFuncPtr func = reinterpret_cast<LibFuncPtr>(GetProcAddress(reinterpret_cast<HMODULE>(a_handle.handle), a_func_name));
	if (func == nullptr)
	{
		LatestOSError();
		BB_ASSERT(false, "Failed to load function from .dll");
	}
	return func;
}

bool BB::WriteToConsole(const char* a_string, uint32_t a_str_length)
{
	DWORD written = 0;
	//Maybe check if a console is available, it could be nullptr.
	return WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), a_string, a_str_length, &written, nullptr);
}

bool BB::WriteToConsole(const wchar_t* a_string, uint32_t a_str_length)
{
	DWORD written = 0;
	return WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE),
		a_string,
		a_str_length,
		&written,
		nullptr);
}

bool BB::OSCreateDirectory(const char* a_path_name)
{
	return CreateDirectoryA(a_path_name, nullptr);
}

bool BB::OSDirectoryExist(const char* a_path_name)
{
	const DWORD attrib = GetFileAttributesA(a_path_name);
	return (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_DIRECTORY));
}

bool BB::OSSetCurrentDirectory(const char* a_path_name)
{
	return SetCurrentDirectoryA(a_path_name);
}

bool BB::OSFileIsValid(const OSFileHandle a_file_handle)
{
	return a_file_handle.handle != reinterpret_cast<uint64_t>(INVALID_HANDLE_VALUE);
}

OSFileHandle BB::OSCreateFile(const char* a_file_name)
{
	const HANDLE created_file = CreateFileA(a_file_name,
		GENERIC_WRITE | GENERIC_READ,
		0,
		nullptr,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		nullptr);

	return OSFileHandle(reinterpret_cast<uintptr_t>(created_file));
}

OSFileHandle BB::OSCreateFile(const wchar* a_file_name)
{
	const HANDLE created_file = CreateFileW(a_file_name,
		GENERIC_WRITE | GENERIC_READ,
		0,
		nullptr,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		nullptr);
	
	return OSFileHandle(reinterpret_cast<uintptr_t>(created_file));
}

OSFileHandle BB::OSLoadFile(const char* a_file_name)
{
	const HANDLE load_file = CreateFileA(a_file_name,
		GENERIC_WRITE | GENERIC_READ,
		0,
		nullptr,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		nullptr);

	return OSFileHandle(reinterpret_cast<uintptr_t>(load_file));
}

//char replaced with string view later on.
OSFileHandle BB::OSLoadFile(const wchar* a_file_name)
{
	const HANDLE load_file = CreateFileW(a_file_name,
		GENERIC_WRITE | GENERIC_READ,
		0,
		nullptr,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		nullptr);

	return OSFileHandle(reinterpret_cast<uintptr_t>(load_file));
}

void BB::ReadOSFile(const OSFileHandle a_file_handle, void* a_memory, const size_t a_memory_size)
{
	DWORD bytes_read = 0;
	if (FALSE == ReadFile(reinterpret_cast<HANDLE>(a_file_handle.handle),
		a_memory,
		static_cast<DWORD>(a_memory_size),
		&bytes_read,
		nullptr))
	{
		LatestOSError();
		BB_WARNING(false,
			"OS, failed to read file!",
			WarningType::HIGH);
	}
}

Buffer BB::ReadOSFile(MemoryArena& a_arena, const OSFileHandle a_file_handle)
{
	Buffer file_buffer{};
	file_buffer.size = GetOSFileSize(a_file_handle);
	file_buffer.data = reinterpret_cast<char*>(ArenaAlloc(a_arena, file_buffer.size, alignof(size_t)));

	ReadOSFile(a_file_handle, file_buffer.data, file_buffer.size);

	return file_buffer;
}

Buffer BB::ReadOSFile(MemoryArena& a_arena, const char* a_path)
{
	OSFileHandle read_file = OSLoadFile(a_path);
	BB_ASSERT(OSFileIsValid(read_file), "OS file invalid, will cause errors");

	Buffer file_buffer;
	file_buffer.size = GetOSFileSize(read_file);
	file_buffer.data = reinterpret_cast<char*>(ArenaAlloc(a_arena, file_buffer.size, alignof(size_t)));

	ReadOSFile(read_file, file_buffer.data, file_buffer.size);

	CloseOSFile(read_file);

	return file_buffer;
}

Buffer BB::ReadOSFile(MemoryArena& a_arena, const wchar* a_path)
{
	OSFileHandle read_file = OSLoadFile(a_path);
	BB_ASSERT(OSFileIsValid(read_file), "OS file invalid, will cause errors");

	Buffer file_buffer;
	file_buffer.size = GetOSFileSize(read_file);
	file_buffer.data = reinterpret_cast<char*>(ArenaAlloc(a_arena, file_buffer.size, alignof(size_t)));

	ReadOSFile(read_file, file_buffer.data, file_buffer.size);

	CloseOSFile(read_file);

	return file_buffer;
}

//char replaced with string view later on.
bool BB::WriteToOSFile(const OSFileHandle a_file_handle, const void* a_data, const size_t a_size)
{
	DWORD bytes_written = 0;
	return WriteFile(reinterpret_cast<HANDLE>(a_file_handle.handle), a_data, static_cast<const DWORD>(a_size), &bytes_written, nullptr);
}

//Get a file's size in bytes.
uint64_t BB::GetOSFileSize(const OSFileHandle a_file_handle)
{
	return GetFileSize(reinterpret_cast<HANDLE>(a_file_handle.handle), nullptr);
}

void BB::SetOSFilePosition(const OSFileHandle a_file_handle, const uint32_t a_offset, const OS_FILE_READ_POINT a_file_read_point)
{
	DWORD err = SetFilePointer(reinterpret_cast<HANDLE>(a_file_handle.handle), static_cast<LONG>(a_offset), nullptr, static_cast<DWORD>(a_file_read_point));
#ifdef _DEBUG
	if (err == INVALID_SET_FILE_POINTER && 
		LatestOSError() == ERROR_NEGATIVE_SEEK)
	{
		BB_WARNING(false,
			"OS, Setting the file position failed by putting it in negative! WIN ERROR: ERROR_NEGATIVE_SEEK.",
			WarningType::HIGH);
	}
#endif //_DEBUG
}

bool BB::OSFileExist(const char* a_path)
{
	if (INVALID_FILE_ATTRIBUTES == GetFileAttributesA(a_path))
	{
		// make sure that the error is nothing else
		BB_ASSERT(GetLastError() == ERROR_FILE_NOT_FOUND, "OSFileExist is false but not related to the file not existing, this should never happen");
		return false;
	}
	return true;
}

// OPENFILENAME changed the directory so I did not like it.
// tutorial for the new file stuff,  https://learn.microsoft.com/en-us/windows/win32/learnwin32/example--the-open-dialog-box
bool BB::OSFindFileNameDialogWindow(char* a_str_buffer, const size_t a_str_buffer_size, const char* a_initial_directory)
{
	OPENFILENAMEA open_file_name{};

	ZeroMemory(&open_file_name, sizeof(OPENFILENAME));
	open_file_name.lStructSize = sizeof(OPENFILENAME);
	open_file_name.lpstrFile = a_str_buffer;
	open_file_name.nMaxFile = static_cast<DWORD>(a_str_buffer_size);
	open_file_name.lpstrFilter = nullptr; // TODO, docs are weird on this
	open_file_name.nFilterIndex = 0; // TODO, docs are weird on this.
	if (a_initial_directory == nullptr)
		open_file_name.lpstrInitialDir = g_exe_path;
	else
		open_file_name.lpstrInitialDir = a_initial_directory;
	open_file_name.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

	if (GetOpenFileNameA(&open_file_name) == TRUE)
	{
		return true;
	}

	return false;
}

bool BB::CloseOSFile(const OSFileHandle a_file_handle)
{
	return CloseHandle(reinterpret_cast<HANDLE>(a_file_handle.handle));
}

OSThreadHandle BB::OSCreateThread(void(*a_func)(void*), const unsigned int a_stack_size, void* a_arg_list)
{
	return OSThreadHandle(_beginthread(a_func, a_stack_size, a_arg_list));
}

bool BB::OSWaitThreadfinish(const OSThreadHandle a_thread)
{
	return WaitForSingleObject(reinterpret_cast<HANDLE>(a_thread.handle), INFINITE);
}

bool BB::OSSetThreadName(const wchar_t* a_wstr)
{
	return SetThreadDescription(GetCurrentThread(), a_wstr);
}

BBMutex BB::OSCreateMutex()
{
	return BBMutex(reinterpret_cast<uintptr_t>(CreateMutex(nullptr, false, nullptr)));
}

bool BB::OSWaitAndLockMutex(const BBMutex a_mutex)
{
	return WaitForSingleObject(reinterpret_cast<HANDLE>(a_mutex.handle), INFINITE);
}

bool BB::OSUnlockMutex(const BBMutex a_mutex)
{
	return ReleaseMutex(reinterpret_cast<HANDLE>(a_mutex.handle));
}

bool BB::OSDestroyMutex(const BBMutex a_mutex)
{
	return CloseHandle(reinterpret_cast<HANDLE>(a_mutex.handle));
}

BBSemaphore BB::OSCreateSemaphore(const uint32_t a_initial_count, const uint32_t a_maximum_count)
{
	return BBSemaphore(reinterpret_cast<uintptr_t>(CreateSemaphore(nullptr, static_cast<LONG>(a_initial_count), static_cast<LONG>(a_maximum_count), nullptr)));
}

bool BB::OSWaitSemaphore(const BBSemaphore a_semaphore)
{
	return WaitForSingleObject(reinterpret_cast<HANDLE>(a_semaphore.handle), INFINITE);
}

bool BB::OSSignalSemaphore(const BBSemaphore a_semaphore, const uint32_t a_signal_count)
{
	return ReleaseSemaphore(reinterpret_cast<HANDLE>(a_semaphore.handle), static_cast<LONG>(a_signal_count), nullptr);
}

bool BB::OSDestroySemaphore(const BBSemaphore a_semaphore)
{
	return CloseHandle(reinterpret_cast<HANDLE>(a_semaphore.handle));
}

BBRWLock BB::OSCreateRWLock()
{
	//-Wbraced-scalar-init warning, lazy ignore.
BB_WARNINGS_OFF
	return BBRWLock(SRWLOCK_INIT);
BB_WARNINGS_ON
}

void BB::OSAcquireSRWLockRead(BBRWLock* a_lock)
{
	AcquireSRWLockShared(reinterpret_cast<SRWLOCK*>(a_lock));
}

void BB::OSAcquireSRWLockWrite(BBRWLock* a_lock)
{
	AcquireSRWLockExclusive(reinterpret_cast<SRWLOCK*>(a_lock));
}

void BB::OSReleaseSRWLockRead(BBRWLock* a_lock)
{
	ReleaseSRWLockShared(reinterpret_cast<SRWLOCK*>(a_lock));
}

void BB::OSReleaseSRWLockWrite(BBRWLock* a_lock)
{
	ReleaseSRWLockExclusive(reinterpret_cast<SRWLOCK*>(a_lock));
}

BBConditionalVariable BB::OSCreateConditionalVariable()
{
	//-Wbraced-scalar-init warning, lazy ignore
	BB_WARNINGS_OFF
	return BBConditionalVariable(CONDITION_VARIABLE_INIT);
	BB_WARNINGS_ON
}

bool BB::OSWaitConditionalVariableShared(BBConditionalVariable* a_condition, BBRWLock* a_lock)
{
	return SleepConditionVariableSRW(reinterpret_cast<CONDITION_VARIABLE*>(a_condition),
		reinterpret_cast<SRWLOCK*>(a_lock), INFINITE, CONDITION_VARIABLE_LOCKMODE_SHARED);
}

bool BB::OSWaitConditionalVariableExclusive(BBConditionalVariable* a_condition, BBRWLock* a_lock)
{
	return SleepConditionVariableSRW(reinterpret_cast<CONDITION_VARIABLE*>(a_condition),
		reinterpret_cast<SRWLOCK*>(a_lock), INFINITE, 0);
}

void BB::OSWakeConditionVariable(BBConditionalVariable* a_condition)
{
	WakeConditionVariable(reinterpret_cast<CONDITION_VARIABLE*>(a_condition));
}

WindowHandle BB::CreateOSWindow(const OS_WINDOW_STYLE a_style, const int a_x, const int a_y, const int a_width, const int a_height, const wchar* a_window_name)
{
	HWND window;
	HINSTANCE hinstance{};

	WNDCLASSW wnd_class = {};
	wnd_class.lpszClassName = a_window_name;
	wnd_class.hInstance = hinstance;
	wnd_class.hIcon = LoadIconW(nullptr, IDI_WINLOGO);
	wnd_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
	wnd_class.lpfnWndProc = WindowProc;

	RegisterClassW(&wnd_class);
	//DWORD style = WS_CAPTION | WS_MINIMIZEBOX | WS_SYSMENU;

	DWORD style;
	switch (a_style)
	{
	case OS_WINDOW_STYLE::MAIN:
		style = WS_OVERLAPPEDWINDOW;
		break;
	case OS_WINDOW_STYLE::CHILD:
		style = WS_OVERLAPPED | WS_THICKFRAME;
		break;
	}

	RECT rect{};
	rect.left = a_x;
	rect.top = a_y;
	rect.right = rect.left + a_width;
	rect.bottom = rect.top + a_height;

	AdjustWindowRect(&rect, style, false);

	window = CreateWindowEx(
		0,
		a_window_name,
		g_program_name,
		style,
		rect.left,
		rect.top,
		rect.right - rect.left,
		rect.bottom - rect.top,
		nullptr,
		nullptr,
		hinstance,
		nullptr);
	ShowWindow(window, SW_SHOW);

	//Get the mouse and keyboard.
	RAWINPUTDEVICE rid[2]{};

	rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
	rid[0].usUsage = HID_USAGE_GENERIC_KEYBOARD;
	rid[0].dwFlags = RIDEV_NOLEGACY;
	rid[0].hwndTarget = window;

	rid[1].usUsagePage = HID_USAGE_PAGE_GENERIC;
	rid[1].usUsage = HID_USAGE_GENERIC_MOUSE;
	rid[1].dwFlags = 0;
	rid[1].hwndTarget = window;

	BB_ASSERT(RegisterRawInputDevices(rid, 2, sizeof(RAWINPUTDEVICE)),
		"Failed to register raw input devices!");

	uint32_t num_connected_devices = 0;
	UINT err_check = GetRawInputDeviceList(nullptr, &num_connected_devices, sizeof(RAWINPUTDEVICELIST));
	BB_ASSERT(err_check != static_cast<UINT>(-1), "Failed to get the size of raw input devices!");
	BB_ASSERT(num_connected_devices > 0, "Failed to get the size of raw input devices!");

	RAWINPUTDEVICELIST* connected_devices = BBstackAlloc(
		num_connected_devices,
		RAWINPUTDEVICELIST);

	err_check = GetRawInputDeviceList(connected_devices, &num_connected_devices, sizeof(RAWINPUTDEVICELIST));
	BB_ASSERT(err_check != static_cast<UINT>(-1), "Failed to get the raw input devices!");
	
	//constexpr size_t MAX_HID_STRING_LENGTH = 126;
	//wchar_t* product_name_str = BBstackAlloc(
	//	MAX_HID_STRING_LENGTH,
	//	wchar_t);

	////Lets log the devices, maybe do this better.
	//for (size_t i = 0; i < num_connected_devices; i++)
	//{
	//	RID_DEVICE_INFO device_info{};
	//	UINT rid_device_size = sizeof(RID_DEVICE_INFO);
	//	UINT it = GetRawInputDeviceInfo(connected_devices[i].hDevice, RIDI_DEVICEINFO, &device_info, &rid_device_size);

	//	if (device_info.dwType == RIM_TYPEMOUSE)
	//	{
	//		
	//	}
	//	else if (device_info.dwType == RIM_TYPEMOUSE)
	//	{

	//	}
	//}

	return WindowHandle(reinterpret_cast<uintptr_t>(window));
}

void* BB::GetOSWindowHandle(const WindowHandle a_handle)
{
	return reinterpret_cast<HWND>(a_handle.handle);
}

bool BB::GetWindowSize(const WindowHandle a_handle, int& a_x, int& a_y)
{
	RECT rect;
	bool ret_val = GetClientRect(reinterpret_cast<HWND>(a_handle.handle), &rect);

	a_x = rect.right;
	a_y = rect.bottom;
	return ret_val;
}

bool BB::DirectDestroyOSWindow(const WindowHandle a_handle)
{
	return DestroyWindow(reinterpret_cast<HWND>(a_handle.handle));
}

bool BB::FreezeMouseOnWindow(const WindowHandle a_handle)
{
	RECT rect;
	BB_ASSERT(GetClientRect(reinterpret_cast<HWND>(a_handle.handle), &rect), "invalid window handle send to FreezeMouseOnWindow");

	POINT left_right_up_down[2]{};
	left_right_up_down[0].x = rect.left;
	left_right_up_down[0].y = rect.top;
	left_right_up_down[1].x = rect.right;
	left_right_up_down[1].y = rect.bottom;

	MapWindowPoints(reinterpret_cast<HWND>(a_handle.handle), nullptr, left_right_up_down, _countof(left_right_up_down));

	rect.left = left_right_up_down[0].x;
	rect.top = left_right_up_down[0].y;

	rect.right = left_right_up_down[1].x;
	rect.bottom = left_right_up_down[1].y;

	return ClipCursor(&rect);
}

bool BB::UnfreezeMouseOnWindow()
{
	return ClipCursor(nullptr);
}

bool BB::OSMessageBoxYesNo(const char* a_box_title, const char* a_message)
{
	if (MessageBoxA(nullptr, a_box_title, a_message, MB_YESNO) == IDYES)
		return true;
	return false;
}

void BB::SetWindowCloseEvent(PFN_WindowCloseEvent a_func)
{
	s_pfn_close_event = a_func;
}

void BB::SetWindowResizeEvent(PFN_WindowResizeEvent a_func)
{
	s_pfn_resize_event = a_func;
}

BB_NO_RETURN void BB::ExitApp()
{
	exit(EXIT_SUCCESS);
}

bool BB::ProcessMessages(const WindowHandle a_window_handle)
{
	//TRACKMOUSEEVENT t_MouseTrackE{};
	//t_MouseTrackE.cbSize = sizeof(TRACKMOUSEEVENT);
	//t_MouseTrackE.dwFlags = TME_LEAVE;
	//t_MouseTrackE.hwndTrack = reinterpret_cast<HWND>(a_window_handle.handle);
	//TrackMouseEvent(&t_MouseTrackE);


	MSG msg{};

	while (PeekMessage(&msg, reinterpret_cast<HWND>(a_window_handle.handle), 0u, 0u, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return true;
}

void BB::PollInputEvents(InputEvent* a_event_buffers, size_t& input_event_amount)
{
	input_event_amount = s_input_buffer.used;
	if (a_event_buffers == nullptr)
		return;
	
	//Overwrite could happen! But this is user's responsibility.
	GetAllInput(a_event_buffers);
}

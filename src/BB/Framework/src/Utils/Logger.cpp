#include "BBGlobal.h"
#include "Logger.h"
#include "Program.h"

#include "BBThreadScheduler.hpp"
#include "Allocators.h"
#include "Storage/BBString.h"
#include <stdarg.h>

using namespace BB;

static void WriteLoggerToFile(void*);

static LinearAllocator_t s_allocator(mbSize * 4, "Log Allocator");

//dirty singleton
class LoggerSingleton
{
private:
	uint32_t m_max_logger_buffer_size;
	WarningTypeFlags m_enabled_warning_flags = 0;
	//create a fixed string class for this.
	String m_cache_string;
	//create a fixed string class for this.
	String m_upload_string;
	ThreadTask m_last_thread_task = ThreadTask(BB_INVALID_HANDLE_64);
	const BBMutex m_write_to_file_mutex;
	const OSFileHandle m_log_file;

	static LoggerSingleton* m_logger_inst;
public:

	LoggerSingleton()
		: m_max_logger_buffer_size(2024),
		  m_cache_string(s_allocator, m_max_logger_buffer_size),
		  m_upload_string(s_allocator, m_max_logger_buffer_size),
	      m_write_to_file_mutex(OSCreateMutex()),
		  m_log_file(CreateOSFile(L"logger.txt"))
	{
		//set them all to true at the start.
		m_enabled_warning_flags = UINT32_MAX;
	}

	~LoggerSingleton()
	{
		//write the last logger information
		LoggerWriteToFile();
		//clear it to avoid issues related to reporting memory leaks.
		s_allocator.Clear();
		OSDestroyMutex(m_write_to_file_mutex);
	}

	static LoggerSingleton* GetInstance()
	{
		if (!m_logger_inst)
			m_logger_inst = BBnew(s_allocator, LoggerSingleton);

		return m_logger_inst;
	}

	void WriteLogInfoToFile(const char* a_msg, const size_t a_size)
	{
		OSWaitAndLockMutex(m_write_to_file_mutex);
		WriteToConsole(a_msg, static_cast<uint32_t>(a_size));

		if (m_cache_string.size() + a_size > m_max_logger_buffer_size)
		{
			m_upload_string.clear();
			m_upload_string.append(m_cache_string);
			//async upload to file.
			if (m_last_thread_task.IsValid())
				Threads::WaitForTask(m_last_thread_task);
			m_last_thread_task = Threads::StartTaskThread(WriteLoggerToFile, nullptr);
			//clear the cache string for new logging infos
			m_cache_string.clear();
		}

		m_cache_string.append(a_msg, a_size);

		OSUnlockMutex(m_write_to_file_mutex);
	}

	void LoggerWriteToFile()
	{
		WriteToOSFile(m_log_file, m_upload_string.data(), m_upload_string.size());
	}

	void EnableLogType(const WarningType a_warning_type)
	{
		m_enabled_warning_flags |= static_cast<WarningTypeFlags>(a_warning_type);
	}

	void EnableLogTypes(const WarningTypeFlags a_warning_types)
	{
		m_enabled_warning_flags = a_warning_types;
	}

	bool IsLogEnabled(const WarningType a_type)
	{
		return (m_enabled_warning_flags & static_cast<WarningTypeFlags>(a_type)) == static_cast<WarningTypeFlags>(a_type);
	}
};
LoggerSingleton* LoggerSingleton::LoggerSingleton::m_logger_inst = nullptr;

static void WriteLoggerToFile(void*)
{
	LoggerSingleton::GetInstance()->LoggerWriteToFile();
}

static void Log_to_Console(const char* a_file_name, int a_line, const char* a_warning_level, const char* a_formats, va_list a_args)
{
	constexpr const char LOG_MESSAGE_ERROR_LEVEL_0[]{ "Severity: " };
	constexpr const char LOG_MESSAGE_FILE_0[]{ "\nFile: " };
	constexpr const char LOG_MESSAGE_LINE_NUMBER_1[]{ "\nLine Number: " };
	constexpr const char LOG_MESSAGE_MESSAGE_TXT_2[]{ "\nThe Message: " };

	//Format the message
	StackString<4096> string{};
	{	//Start with the warning level
		string.append(LOG_MESSAGE_ERROR_LEVEL_0, sizeof(LOG_MESSAGE_ERROR_LEVEL_0) - 1);
		string.append(a_warning_level);
	}
	{ //Get the file.
		string.append(LOG_MESSAGE_FILE_0, sizeof(LOG_MESSAGE_FILE_0) - 1);
		string.append(a_file_name);
	}
	{ //Get the line number into the buffer
		char lineNumString[8]{};
		sprintf_s(lineNumString, 8, "%u", a_line);

		string.append(LOG_MESSAGE_LINE_NUMBER_1, sizeof(LOG_MESSAGE_LINE_NUMBER_1) - 1);
		string.append(lineNumString);
	}
	{ //Get the message(s).
		string.append(LOG_MESSAGE_MESSAGE_TXT_2, sizeof(LOG_MESSAGE_MESSAGE_TXT_2) - 1);
		for (size_t i = 0; i < static_cast<size_t>(a_formats[i] != '\0'); i++)
		{
			switch (a_formats[i])
			{
			case 's':
				string.append(va_arg(a_args, char*));
				break;
			case 'S': //convert it to a char first.
			{
				const wchar_t* w_char = va_arg(a_args, const wchar_t*);
				const size_t char_size = wcslen(w_char);
				//check to see if we do not go over bounds, largly to deal with non-null-terminated wchar strings.
				BB_ASSERT(char_size < string.capacity() - string.size(), "error log string size exceeds 1024 characters!");
				char* character = BBstackAlloc_s(char_size, char);
				size_t conv_chars = 0;
				wcstombs_s(&conv_chars, character, char_size, w_char, _TRUNCATE);
				string.append(character);
				BBstackFree_s(character);
			}
				break;
			default:
				BB_ASSERT(false, "va arg format not yet supported");
				break;
			}

			string.append(" ", 1);
		}
		//double line ending for better reading.
		string.append("\n\n", 2);
	}

	Buffer log_buffer{};
	log_buffer.data = string.data();
	log_buffer.size = string.size();

	LoggerSingleton::GetInstance()->WriteLogInfoToFile(string.data(), string.size());
}

void Logger::Log_Message(const char* a_file_name, int a_line, const char* a_formats, ...)
{
	if (!LoggerSingleton::GetInstance()->IsLogEnabled(WarningType::INFO))
		return;
	va_list vl;
	va_start(vl, a_formats);
	Log_to_Console(a_file_name, a_line, "Info", a_formats, vl);
	va_end(vl);
}

void Logger::Log_Warning_Optimization(const char* a_file_name, int a_line, const char* a_formats, ...)
{
	if (!LoggerSingleton::GetInstance()->IsLogEnabled(WarningType::OPTIMALIZATION))
		return;
	va_list vl;
	va_start(vl, a_formats);
	Log_to_Console(a_file_name, a_line, "Optimalization Warning", a_formats, vl);
	va_end(vl);
}

void Logger::Log_Warning_Low(const char* a_file_name, int a_line, const char* a_formats, ...)
{
	if (!LoggerSingleton::GetInstance()->IsLogEnabled(WarningType::LOW))
		return;
	va_list vl;
	va_start(vl, a_formats);
	Log_to_Console(a_file_name, a_line, "Warning (LOW)", a_formats, vl);
	va_end(vl);
}

void Logger::Log_Warning_Medium(const char* a_file_name, int a_line, const char* a_formats, ...)
{
	if (!LoggerSingleton::GetInstance()->IsLogEnabled(WarningType::MEDIUM))
		return;
	va_list vl;
	va_start(vl, a_formats);
	Log_to_Console(a_file_name, a_line, "Warning (MEDIUM)", a_formats, vl);
	va_end(vl);
}

void Logger::Log_Warning_High(const char* a_file_name, int a_line, const char* a_formats, ...)
{
	if (!LoggerSingleton::GetInstance()->IsLogEnabled(WarningType::HIGH))
		return;
	va_list vl;
	va_start(vl, a_formats);
	Log_to_Console(a_file_name, a_line, "Warning (HIGH)", a_formats, vl);
	va_end(vl);
}

void Logger::Log_Assert(const char* a_file_name, int a_line, const char* a_formats, ...)
{
	if (!LoggerSingleton::GetInstance()->IsLogEnabled(WarningType::ASSERT))
		return;
	va_list vl;
	va_start(vl, a_formats);
	Log_to_Console(a_file_name, a_line, "Critical", a_formats, vl);
	va_end(vl);
}

void Logger::EnableLogType(const WarningType a_warning_type)
{
	LoggerSingleton::GetInstance()->EnableLogType(a_warning_type);
}

void Logger::EnableLogTypes(const WarningTypeFlags a_warning_types)
{
	LoggerSingleton::GetInstance()->EnableLogTypes(a_warning_types);
}

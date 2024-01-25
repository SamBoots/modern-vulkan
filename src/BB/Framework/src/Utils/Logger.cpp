#include "BBGlobal.h"
#include "Logger.h"
#include "Program.h"

#include "BBThreadScheduler.hpp"
#include "Storage/BBString.h"
#include <stdarg.h>

using namespace BB;

void Logger::LoggerWriteToFile()
{
	WriteToOSFile(g_logger.log_file, g_logger.upload_string.data(), g_logger.upload_string.size());
}

static void LoggerWriteToFile_async(void*)
{
	Logger::LoggerWriteToFile();
}

static void WriteLogInfoToFile(const char* a_msg, const size_t a_size)
{
	OSWaitAndLockMutex(g_logger.write_to_file_mutex);
	WriteToConsole(a_msg, static_cast<uint32_t>(a_size));

	if (g_logger.cache_string.size() + a_size > g_logger.max_logger_buffer_size)
	{
		g_logger.upload_string.clear();
		g_logger.upload_string.append(g_logger.cache_string);
		if (g_logger.last_thread_task.IsValid())
		{
			Threads::WaitForTask(g_logger.last_thread_task);
		}
		// don't use the last thread handle, this bool should be sync enough for this instance.
		Threads::StartTaskThread(LoggerWriteToFile_async, nullptr);
		g_logger.cache_string.clear();
	}

	g_logger.cache_string.append(a_msg, a_size);

	OSUnlockMutex(g_logger.write_to_file_mutex);
}

static bool IsLogEnabled(const WarningType a_type)
{
	return (g_logger.enabled_warning_flags & static_cast<WarningTypeFlags>(a_type)) == static_cast<WarningTypeFlags>(a_type);
}

static void Log_to_Console(const char* a_file_name, int a_line, const char* a_warning_level, const char* a_formats, va_list a_args)
{
	constexpr const char LOG_MESSAGE_ERROR_LEVEL_0[]{ "Severity: " };
	constexpr const char LOG_MESSAGE_FILE_0[]{ "\nFile: " };
	constexpr const char LOG_MESSAGE_LINE_NUMBER_1[]{ "\nLine Number: " };
	constexpr const char LOG_MESSAGE_MESSAGE_TXT_2[]{ "\nThe Message: " };

	//Format the message
	StackString<8192> string{};
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
		for (size_t i = 0; static_cast<size_t>(a_formats[i] != '\0'); i++)
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

	WriteLogInfoToFile(string.data(), string.size());
}

void Logger::Log_Message(const char* a_file_name, int a_line, const char* a_formats, ...)
{
	if (!IsLogEnabled(WarningType::INFO))
		return;
	va_list vl;
	va_start(vl, a_formats);
	Log_to_Console(a_file_name, a_line, "Info", a_formats, vl);
	va_end(vl);
}

void Logger::Log_Warning_Optimization(const char* a_file_name, int a_line, const char* a_formats, ...)
{
	if (!IsLogEnabled(WarningType::OPTIMALIZATION))
		return;
	va_list vl;
	va_start(vl, a_formats);
	Log_to_Console(a_file_name, a_line, "Optimalization Warning", a_formats, vl);
	va_end(vl);
}

void Logger::Log_Warning_Low(const char* a_file_name, int a_line, const char* a_formats, ...)
{
	if (!IsLogEnabled(WarningType::LOW))
		return;
	va_list vl;
	va_start(vl, a_formats);
	Log_to_Console(a_file_name, a_line, "Warning (LOW)", a_formats, vl);
	va_end(vl);
}

void Logger::Log_Warning_Medium(const char* a_file_name, int a_line, const char* a_formats, ...)
{
	if (!IsLogEnabled(WarningType::MEDIUM))
		return;
	va_list vl;
	va_start(vl, a_formats);
	Log_to_Console(a_file_name, a_line, "Warning (MEDIUM)", a_formats, vl);
	va_end(vl);
}

void Logger::Log_Warning_High(const char* a_file_name, int a_line, const char* a_formats, ...)
{
	if (!IsLogEnabled(WarningType::HIGH))
		return;
	va_list vl;
	va_start(vl, a_formats);
	Log_to_Console(a_file_name, a_line, "Warning (HIGH)", a_formats, vl);
	va_end(vl);
}

void Logger::Log_Assert(const char* a_file_name, int a_line, const char* a_formats, ...)
{
	if (!IsLogEnabled(WarningType::ASSERT))
		return;
	va_list vl;
	va_start(vl, a_formats);
	Log_to_Console(a_file_name, a_line, "Critical", a_formats, vl);
	va_end(vl);
}

void Logger::EnableLogType(const WarningType a_warning_type)
{
	g_logger.enabled_warning_flags |= static_cast<WarningTypeFlags>(a_warning_type);
}

void Logger::EnableLogTypes(const WarningTypeFlags a_warning_types)
{
	g_logger.enabled_warning_flags = a_warning_types;
}

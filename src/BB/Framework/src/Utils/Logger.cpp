#include "BBGlobal.h"
#include "Logger.h"
#include "Program.h"

#include "BBThreadScheduler.hpp"
#include "Storage/BBString.h"
#include <stdarg.h>

using namespace BB;

static void StandardLogCallback(const char* a_file_name, const int a_line, const WarningType a_warning_type, const char* a_formats, void*, va_list a_args)
{
	StackString<2048> string{};
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
	Logger::LogToConsole(a_file_name, a_line, a_warning_type, string.c_str());
}

static PFN_LoggerCallback logger_callback = StandardLogCallback;
static void* logger_userdata = nullptr;

void Logger::LoggerSetCallback(const PFN_LoggerCallback a_callback, void* a_userdata)
{
	logger_callback = a_callback;
	logger_userdata = a_userdata;
}

const char* Logger::WarningTypeToCChar(const WarningType a_type)
{
	switch (a_type)
	{
	case BB::WarningType::INFO:
		return "INFO";
	case BB::WarningType::OPTIMIZATION:
		return "OPTIMIZATION";
	case BB::WarningType::LOW:
		return "LOW";
	case BB::WarningType::MEDIUM:
		return "MEDIUM";
	case BB::WarningType::HIGH:
		return "HIGH";
	case BB::WarningType::ASSERT:
		return "HIGH";
	default:
		BB_ASSERT(false, "error finding warning type");
		return "ERROR";
	}
}

void Logger::LogToConsole(const char* a_file_name, int a_line, const WarningType a_warning_type, const char* a_message)
{
	constexpr const char LOG_MESSAGE_ERROR_LEVEL_0[]{ "Severity: " };
	constexpr const char LOG_MESSAGE_FILE_0[]{ "\nFile: " };
	constexpr const char LOG_MESSAGE_LINE_NUMBER_1[]{ "\nLine Number: " };
	constexpr const char LOG_MESSAGE_MESSAGE_TXT_2[]{ "\nThe Message: " };

	// format the message
	StackString<8192> string{};
	// start with the warning level
	string.append(LOG_MESSAGE_ERROR_LEVEL_0, sizeof(LOG_MESSAGE_ERROR_LEVEL_0) - 1);
	string.append(Logger::WarningTypeToCChar(a_warning_type));

	// get the file.
	string.append(LOG_MESSAGE_FILE_0, sizeof(LOG_MESSAGE_FILE_0) - 1);
	string.append(a_file_name);

	// get the line number into the buffer
	char lineNumString[8]{};
	sprintf_s(lineNumString, 8, "%u", a_line);
	string.append(LOG_MESSAGE_LINE_NUMBER_1, sizeof(LOG_MESSAGE_LINE_NUMBER_1) - 1);
	string.append(lineNumString);

	// get message
	string.append(LOG_MESSAGE_MESSAGE_TXT_2, sizeof(LOG_MESSAGE_MESSAGE_TXT_2) - 1);
	string.append(a_message);

	// skip two lines.
	string.append("\n\n", 2);

	WriteToConsole(string.c_str(), static_cast<uint32_t>(string.size()));
}

void Logger::LogMessage(const char* a_file_name, const int a_line, const WarningType a_type, const char* a_formats, ...)
{
	va_list vl;
	va_start(vl, a_formats);
	logger_callback(a_file_name, a_line, a_type, a_formats, logger_userdata, vl);
	va_end(vl);
}

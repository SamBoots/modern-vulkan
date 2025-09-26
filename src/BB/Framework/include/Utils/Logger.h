#pragma once
#include <cassert>
#include "Common.h"

namespace BB
{
	typedef void (*PFN_LoggerCallback)(const char* a_file_name, const int a_line, const WarningType a_warning_type, const char* a_formats, void* a_puserdata, va_list a_args);

	namespace Logger
	{
		void LoggerSetCallback(const PFN_LoggerCallback a_callback, void* a_userdata);

		const char* WarningTypeToCChar(const WarningType a_type);

		void LogToConsole(const char* a_file_name, int a_line, const WarningType a_warning_type, const char* a_message);
		void LogMessage(const char* a_file_name, const int a_line, const WarningType a_type, const char* a_formats, ...);
        void LogToMessageBox(const char* a_title, const char* a_string);
	}
}


#define BB_LOG(a_msg) BB::Logger::LogMessage(__FILE__, __LINE__, WarningType::INFO, a_msg)
#define BB_LOGF(a_msg, ...) BB::Logger::LogMessage(__FILE__, __LINE__, WarningType::INFO, a_msg, __VA_ARGS__)

#ifdef _DEBUG
	// Check for unintented behaviour at compile time, if a_check is false the program will stop and post a message.
#define BB_STATIC_ASSERT(a_check, a_msg) static_assert(a_check, a_msg)

	//  Check for unintented behaviour at runetime, if a_check is false the program will stop and post the message a_msg.
#define BB_ASSERT(a_check, a_msg, ...)	\
	do						\
	{						\
		if (!(a_check))		\
		{					\
			BB::Logger::LogMessage(__FILE__, __LINE__, WarningType::ASSERT, a_msg); \
			assert(false);	\
		}					\
	} while (0)
#define BB_ASSERTF(a_check, a_msg, ...)	\
	do						\
	{						\
		if (!(a_check))		\
		{					\
			BB::Logger::LogMessage(__FILE__, __LINE__, WarningType::ASSERT, a_msg, __VA_ARGS__); \
			assert(false);	\
		}					\
	} while (0)

	// Post a compile message with "implement: text" and asserts when you hit this code.
#define BB_UNIMPLEMENTED(text)						\
	do												\
	{												\
		BB::Logger::LogToMessageBox("unimplemented code", text);	\
	} while (0) 

	// Check for unintented behaviour at runtime, if a_check is false the program will post a warning message.
#define BB_WARNING(a_check, a_msg, a_warning_type) \
	do																			\
	{																			\
		if (!(a_check))															\
		{																		\
			BB::Logger::LogMessage(__FILE__, __LINE__, a_warning_type, a_msg);  \
		}																		\
	} while (0) 

#define BB_WARNINGF(a_check, a_msg, a_warning_type, ...) \
	do																			\
	{																			\
		if (!(a_check))															\
		{																		\
			BB::Logger::LogMessage(__FILE__, __LINE__, a_warning_type, a_msg, __VA_ARGS__);\
		}																		\
	} while (0) 
#else
/*  Check for unintented behaviour at compile time, if a_Check is false the program will stop and post a message.
	@param a_Check, If false the program will print the message and assert.
	@param a_msg, The message that will be printed. */
#define BB_STATIC_ASSERT(a_Check, a_msg)
/*  Check for unintented behaviour at runtime, if a_Check is false the program will post a warning message.
@param a_Check, If false the program will print the message and assert.
@param a_msg, The message that will be printed. */
#define BB_ASSERT(a_check, a_msg)
#define BB_ASSERTF(a_check, a_msg)
#define BB_UNIMPLEMENTED(a_msg)
/*  Check for unintented behaviour at runtime, if a_Check is false the program will post a warning message.
@param a_Check, If false the program will print the message and assert.
@param a_msg, The message that will be printed.
@param a_WarningType, The warning level, enum found at WarningType. */
#define BB_WARNING(a_Check, a_msg, a_WarningType)
#define BB_WARNINGF(a_Check, a_msg, a_WarningType)
#endif //_DEBUG

#pragma once
#include <cassert>
#include "Common.h"

namespace BB
{
	namespace Logger
	{
		//Is done automatically, but can be useful if you exit the application, this is also handled by DestroyBB()
		void LoggerWriteToFile();

		//Use BB_LOG for better use.
		void Log_Message(const char* a_file_name, int a_line, const char* a_formats, ...);
		//Use BB_WARNING for better use.
		void Log_Warning_Optimization(const char* a_file_name, int a_line, const char* a_formats, ...);
		//Use BB_WARNING for better use.
		void Log_Warning_Low(const char* a_file_name, int a_line, const char* a_formats, ...);
		//Use BB_WARNING for better use.
		void Log_Warning_Medium(const char* a_file_name, int a_line, const char* a_formats, ...);
		//Use BB_WARNING for better use.
		void Log_Warning_High(const char* a_file_name, int a_line, const char* a_formats, ...);
		//Use BB_ASSERT for better use.
		//IT DOES NOT ASSERT, BB_ASSERT DOES
		void Log_Assert(const char* a_file_name, int a_line, const char* a_formats, ...);

		void EnableLogType(const WarningType a_WarningType);
		void EnableLogTypes(const WarningTypeFlags a_WarningTypes);
	}
}

#define BB_LOG(a_msg) BB::Logger::Log_Message(__FILE__, __LINE__, "s", a_msg)

#ifdef _DEBUG
		/*  Check for unintented behaviour at compile time, if a_Check is false the program will stop and post a message.
			@param a_Check, If false the program will print the message and assert.
			@param a_msg, The message that will be printed. */
#define BB_STATIC_ASSERT(a_Check, a_msg) static_assert(a_Check, a_msg)

			/*  Check for unintented behaviour at runetime, if a_Check is false the program will stop and post a message.
			@param a_Check, If false the program will print the message and assert.
			@param a_msg, The message that will be printed. */
#define BB_ASSERT(a_check, a_msg)	\
			do						\
			{						\
				if (!(a_check))		\
				{					\
					BB::Logger::Log_Assert(__FILE__, __LINE__, "s", a_msg); \
					assert(false);	\
				}					\
			} while (0)

#define BB_UNIMPLEMENTED(text)								\
			__pragma(message("implement this: " text))		\
			BB_ASSERT(false, "unimplemented code hit")	
			/*  Check for unintented behaviour at runtime, if a_Check is false the program will post a warning message.
			@param a_Check, If false the program will print the message and assert.
			@param a_msg, The message that will be printed.
			@param a_WarningType, The warning level, enum found at WarningType. */
#define BB_WARNING(a_Check, a_msg, a_WarningType) \
	do \
	{ \
		if (!(a_Check)) \
		{ \
			WarningType TYPE{}; switch (a_WarningType) { \
			case WarningType::INFO: \
				BB::Logger::Log_Message(__FILE__, __LINE__, "s", a_msg); \
				break; \
			case WarningType::OPTIMALIZATION: \
				BB::Logger::Log_Warning_Optimization(__FILE__, __LINE__, "s", a_msg); \
				break; \
			case WarningType::LOW: \
				BB::Logger::Log_Warning_Low(__FILE__, __LINE__, "s", a_msg); \
				break; \
			case WarningType::MEDIUM: \
				BB::Logger::Log_Warning_Medium(__FILE__, __LINE__, "s", a_msg); \
				break; \
			case WarningType::HIGH: \
				BB::Logger::Log_Warning_High(__FILE__, __LINE__, "s", a_msg); \
				break; \
			case WarningType::ASSERT: \
				BB::Logger::Log_Assert(__FILE__, __LINE__, "s", a_msg); \
				break; \
			}; TYPE; \
		} \
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
#define BB_UNIMPLEMENTED()
/*  Check for unintented behaviour at runtime, if a_Check is false the program will post a warning message.
@param a_Check, If false the program will print the message and assert.
@param a_msg, The message that will be printed.
@param a_WarningType, The warning level, enum found at WarningType. */
#define BB_WARNING(a_Check, a_msg, a_WarningType)
#endif //_DEBUG

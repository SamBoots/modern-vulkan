#pragma once
#include "BBPragma.h"

#ifdef __clang__
#define BB_PAD(n)   BB_PRAGMA(clang diagnostic push) \
					BB_PRAGMA(clang diagnostic ignored "-Wunused-private-field") \
	                unsigned char BB_CONCAT_EXPAND(_padding_, __LINE__)[n] \
					BB_PRAGMA(clang diagnostic pop)

#define BB_NO_RETURN			__attribute__((noreturn))

#define BB_WARNINGS_OFF			BB_PRAGMA(clang diagnostic push) \
								BB_PRAGMA(clang diagnostic ignored "-Wall")	\
								BB_PRAGMA(clang diagnostic ignored "-Wextra") \
								BB_PRAGMA(clang diagnostic ignored "-Weverything") \
								BB_PRAGMA(clang diagnostic ignored "-Wpedantic")

#define BB_WARNINGS_ON			BB_PRAGMA(clang diagnostic pop)

#define BB_WARNING_PUSH                     BB_PRAGMA(clang diagnostic push)
#define BB_WARNING_IGNORE_LANGUAGE_TOKEN    BB_PRAGMA(clang diagnostic ignored "-Wlanguage-extension-token")
#define BB_WARNING_POP                      BB_PRAGMA(clang diagnostic pop)

#elif _MSC_VER
#define BB_PAD(n) unsigned char BB_CONCAT_EXPAND(_padding_, __LINE__)[n]

#define BB_NO_RETURN			__declspec(noreturn)

#define BB_WARNINGS_OFF			BB_PRAGMA(warning(push, 0))
#define BB_WARNINGS_ON			BB_PRAGMA(warning(pop, 0))

#define BB_WARNING_PUSH                     BB_PRAGMA(warning(push))
#define BB_WARNING_IGNORE_LANGUAGE_TOKEN
#define BB_WARNING_POP                      BB_PRAGMA(warning(pop))

#endif

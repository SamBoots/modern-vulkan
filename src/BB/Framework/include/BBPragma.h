#pragma once

#define BB_CONCAT(a, b) a##b
#define BB_CONCAT_EXPAND(a, b) BB_CONCAT(a, b)
#define ENUM_CASE_STR(name, entry_name) case name::entry_name: return #entry_name
#define ENUM_CASE_STR_NOT_FOUND() default: return "ENUM_NAME_NOT_FOUND"

#define BB_PRAGMA(X)			_Pragma(#X)
#define BB_PRAGMA_PACK_PUSH(n)  BB_PRAGMA(pack(push,n))
#define BB_PRAGMA_PACK_POP()    BB_PRAGMA(pack(pop))

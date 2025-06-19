#pragma once

#define BB_CREATE_ENUM_CLASS_HELPER_(name, value) name = value,
#define BB_CREATE_ENUM_CLASS_(ENUM_NAME, ENUM_TYPE, ENTRIES) \
enum class ENUM_NAME : ENUM_TYPE \
{ \
    ENTRIES(BB_CREATE_ENUM_CLASS_HELPER_) \
};

#define BB_CREATE_ENUM_TO_STR_SWITCH_HELPER(name, value, ENUM_NAME) \
case ENUM_NAME::name: return #name;

#define BB_CREATE_ENUM_TO_STR_(ENUM_NAME, ENTRIES) \
static inline const char* ENUM_NAME##_STR(const ENUM_NAME a_entry) \
{ \
    switch (a_entry) \
    { \
        ENTRIES(BB_CREATE_ENUM_TO_STR_SWITCH_HELPER, ENUM_NAME) \
        ENUM_CASE_STR_NOT_FOUND(); \
    } \
};

#define BB_CREATE_STR_TO_ENUM_SWITCH_HELPER(name, value, ENUM_NAME) \
if (strcmp(a_entry, #name) == 0) return ENUM_NAME::name;

#define BB_CREATE_STR_TO_ENUM_(ENUM_NAME, ENTRIES) \
static inline ENUM_NAME STR_TO_##ENUM_NAME(const char* a_entry) \
{ \
    ENTRIES(BB_CREATE_STR_TO_ENUM_SWITCH_HELPER, ENUM_NAME) \
    return static_cast<ENUM_NAME>(0); \
};

#define BB_CREATE_ENUM(ENUM_NAME, ENUM_TYPE, ENTRIES) \
BB_CREATE_ENUM_CLASS_(ENUM_NAME, ENUM_TYPE, ENTRIES) \
BB_CREATE_ENUM_TO_STR_(ENUM_NAME, ENTRIES) \
BB_CREATE_STR_TO_ENUM_(ENUM_NAME, ENTRIES) 

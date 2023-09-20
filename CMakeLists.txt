###########################################
#  Top-level of the modern vulkan engine  #
###########################################
cmake_minimum_required (VERSION 3.8)
project ("Modern Vulkan")
set (CMAKE_CXX_STANDARD 17)

add_definitions(-DUNICODE -D_UNICODE -UNICODE)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

#find the CPU architecture type.
if (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "X86")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(CPU_ARCHITECTURE "x64")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(CPU_ARCHITECTURE "x86")
    endif()
    message(STATUS "CMAKE_SYSTEM_PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR}")
elseif (${CMAKE_SYSTEM_PROCESSOR}  STREQUAL "AMD64")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(CPU_ARCHITECTURE "x64")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(CPU_ARCHITECTURE "x86")
    endif()
    message(STATUS "CMAKE_SYSTEM_PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR}")
elseif(${CMAKE_SYSTEM_PROCESSOR}  STREQUAL "ARM64")
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(CPU_ARCHITECTURE "arm64")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(CPU_ARCHITECTURE "arm")
    endif()
    message(STATUS "CMAKE_SYSTEM_PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR}")
else()
message(FATAL_ERROR "CPU Architecture not supported, architecture of this PC: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    add_compile_definitions(PUBLIC _64BIT)
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    add_compile_definitions(PUBLIC _32BIT)
endif()

if (MSVC)
    # warning level 4 and all warnings as errors
    add_compile_definitions(PUBLIC /W4 /WX)
elseif(CMAKE_COMPILER_IS_GNUCXX)
    # lots of warnings and all warnings as errors
    add_compile_definitions(PUBLIC -Wall -Wextra)
endif()

#compile Definitions.
if (WIN32)
add_compile_definitions(PUBLIC _WIN32)
elseif(UNIX)
add_compile_definitions(PUBLIC _LINUX)
endif ()

if (CMAKE_BUILD_TYPE MATCHES Debug)
message("DEBUGGING BUILD SELECTED")
add_compile_definitions(PUBLIC _DEBUG)
elseif(CMAKE_BUILD_TYPE MATCHES Release)
message("RELEASE BUILD SELECTED")
add_compile_definitions(PUBLIC _RELEASE)
elseif(CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)
message("RELEASE WITH DEBUG BUILD SELECTED")
add_compile_definitions(PUBLIC _RELEASE)
else()
message(FATAL_ERROR "UNKNOWN BUILD TYPE")
endif(CMAKE_BUILD_TYPE MATCHES Debug)

add_subdirectory ("lib")
add_subdirectory ("src")

message(STATUS "Building binaries at")
message(STATUS ${CMAKE_BINARY_DIR})
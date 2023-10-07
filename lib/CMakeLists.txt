###################################################
#  This cmakelist handles all external libraries  #
###################################################
cmake_minimum_required (VERSION 3.8)

#header Libs
add_library(VMA INTERFACE
"VMA/vk_mem_alloc.h")
target_include_directories(VMA INTERFACE VMA)

add_library(DXCompiler INTERFACE
"DXC/inc/dxcapi.h"
"DXC/inc/d3d12shader.h")
target_include_directories(DXCompiler INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory("imgui")
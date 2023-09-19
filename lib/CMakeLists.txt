###################################################
#  This cmakelist handles all external libraries  #
###################################################
cmake_minimum_required (VERSION 3.8)

#header Libs
add_library(VMA INTERFACE
"VMA/vk_mem_alloc.h")
target_include_directories(VMA INTERFACE VMA)

add_library(stb_image INTERFACE
"stb/stb_image.h"
)

add_subdirectory("imgui")
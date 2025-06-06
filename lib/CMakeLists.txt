###################################################
#  This cmakelist handles all external libraries  #
###################################################
cmake_minimum_required (VERSION 3.8)

#header Libs
add_library(VMA INTERFACE
"vma/vk_mem_alloc.h")
target_include_directories(VMA INTERFACE VMA)

add_library(DXCompiler INTERFACE
"DXC/inc/dxcapi.h"
"DXC/inc/d3d12shader.h")
target_include_directories(DXCompiler INTERFACE DXC)

add_library(cgltf INTERFACE
"cgltf/cgltf.h")
target_include_directories(cgltf INTERFACE cgltf)

add_library(stb_image INTERFACE
"stb/stb_image.h"
"stb/stb_image_write.h"
"stb/stb_image_resize2.h")
target_include_directories(stb_image INTERFACE stb)

add_library(mikktspace STATIC
"mikktspace/mikktspace.c")
target_include_directories(mikktspace SYSTEM PUBLIC mikktspace)

add_subdirectory("imgui")

add_subdirectory("lua")

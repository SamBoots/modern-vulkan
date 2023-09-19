#############################
#  Renderer.lib for Vulkan  #
#############################
cmake_minimum_required (VERSION 3.8)

find_package(Vulkan REQUIRED)

add_library(Renderer
"Vulkan/VulkanRenderer.cpp")

#Include library
target_include_directories(Renderer PUBLIC
"Vulkan"
)

target_link_libraries(Renderer PUBLIC
	BBFramework 
	Vulkan::Vulkan
)
#############################################################
#  This cmakelist handles the base setup for Modern Vulkan  #
#############################################################
cmake_minimum_required (VERSION 3.8)

add_subdirectory ("BB")

find_package(Vulkan REQUIRED)

add_executable(Renderer
	"Render/Vulkan/VulkanRenderer.cpp"
	"Render/Renderer.cpp"
	"main.cpp")

target_link_libraries(Renderer
	BBFramework 
	Vulkan::Vulkan
	VMA
	IMGUI)

#Include library
target_include_directories(Renderer PUBLIC
	"Render/Vulkan"
	"Render")

add_executable(image_filter_test
	"image_test_main.cpp")

target_link_libraries(image_filter_test PUBLIC
	BBFramework)

add_subdirectory ("resources")
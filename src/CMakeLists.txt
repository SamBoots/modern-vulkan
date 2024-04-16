#############################################################
#  This cmakelist handles the base setup for Modern Vulkan  #
#############################################################
cmake_minimum_required (VERSION 3.8)

add_subdirectory ("BB")
add_subdirectory ("Engine")
add_subdirectory ("Render")
add_subdirectory ("Editor")

add_executable(image_filter_test
	"image_test_main.cpp")

target_link_libraries(image_filter_test PUBLIC
	BBFramework) 

#############################################################
#  This cmakelist handles the base setup for Modern Vulkan  #
#############################################################
cmake_minimum_required (VERSION 3.8)

add_subdirectory ("BB")
#add_subdirectory ("Render")


add_executable(app 
"main.cpp")

#Include library
target_include_directories(app PUBLIC
"BB"
)

target_link_libraries(app
	BBFramework 
)

add_subdirectory ("resources")
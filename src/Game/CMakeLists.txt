#############################################################
#  This cmakelist handles the Editor app for Modern Vulkan  #
#############################################################
cmake_minimum_required (VERSION 3.8)

add_library(Game 
	"GameMain.cpp"
	 
	"RenderViewport.cpp" )

target_include_directories(Game PUBLIC
	"")	# this gets the current directory

target_link_libraries(Game PUBLIC
	Engine)

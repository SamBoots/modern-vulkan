###############################################################
#  This cmakelist handles the Engine setup for Modern Vulkan  #
###############################################################
cmake_minimum_required (VERSION 3.8)

add_library(Engine
	"Transform.cpp"
	"AssetLoader.cpp"
	"SceneHierarchy.cpp" 
	"MaterialSystem.cpp" 
	"EngineConfig.cpp"
	"Viewport.cpp"
	)
	
target_include_directories(Engine PUBLIC
	"")	# this gets the current directory

target_link_libraries(Engine 
	mikktspace
	cgltf
	stb_image
	Renderer)

#############################################################
#  This cmakelist handles the Editor app for Modern Vulkan  #
#############################################################
cmake_minimum_required (VERSION 3.8)

add_executable(Editor
	"Editor.cpp"
	"main.cpp" 
	"Console.cpp" 
    "Gizmo.cpp"
	"ProfilerWindow.cpp" 
    "FreeCamera.cpp" "EditorGame.cpp")

target_compile_definitions(Editor PRIVATE EDITOR_SRC_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}/\")

target_link_libraries(Editor
	Game) 

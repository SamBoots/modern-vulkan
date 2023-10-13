#############################################################
#  This cmakelist handles the base setup for Modern Vulkan  #
#############################################################
cmake_minimum_required (VERSION 3.8)

add_subdirectory ("BB")

find_package(Vulkan REQUIRED)

add_library(Renderer
	"Render/Vulkan/VulkanRenderer.cpp"
	"Render/ShaderCompiler.cpp"
	"Render/Renderer.cpp")

target_link_libraries(Renderer BBFramework 
	Vulkan::Vulkan
	VMA
	IMGUI
	DXCompiler
    "${CMAKE_SOURCE_DIR}/lib/DXC/lib/${CPU_ARCHITECTURE}/dxcompiler.lib")

#compile Definitions.
if (WIN32)
set(PLATFORM_NAME "_WIN")
#target_compile_definitions(Renderer PUBLIC VK_USE_PLATFORM_WIN32_KHR)
elseif(UNIX)
set(PLATFORM_NAME "_LINUX")
target_compile_definitions(Renderer PUBLIC VK_USE_PLATFORM_XLIB_KHR)
endif ()

#Include library
target_include_directories(Renderer PUBLIC
	"Render/Vulkan"
	"Render"
	"../resources/shaders/HLSL" #for the shared shader headers
)

add_custom_command(TARGET Renderer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_SOURCE_DIR}/lib/DXC/bin/${CPU_ARCHITECTURE}/dxcompiler.dll"
        $<TARGET_FILE_DIR:Renderer>)

add_custom_command(TARGET Renderer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_SOURCE_DIR}/lib/DXC/bin/${CPU_ARCHITECTURE}/dxil.dll"
        $<TARGET_FILE_DIR:Renderer>)

add_executable(Engine
	"Engine/Camera.cpp"
	"Engine/Transform.cpp"
	"Engine/AssetLoader.cpp"
	"main.cpp")

target_include_directories(Engine PUBLIC
	"Engine")

target_link_libraries(Engine 
	Renderer)

add_executable(image_filter_test
	"image_test_main.cpp")

target_link_libraries(image_filter_test PUBLIC
	BBFramework)
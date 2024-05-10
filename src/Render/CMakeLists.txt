#################################################################
#  This cmakelist handles the Renderer setup for Modern Vulkan  #
#################################################################
cmake_minimum_required (VERSION 3.8)

find_package(Vulkan REQUIRED)

add_library(Renderer
	"Vulkan/VulkanRenderer.cpp"
	"ShaderCompiler.cpp"
	"Renderer.cpp" 
	"GPUBuffers.cpp")

target_link_libraries(Renderer BBFramework 
	Vulkan::Vulkan
	VMA
	IMGUI
	DXCompiler
    "${CMAKE_SOURCE_DIR}/lib/DXC/lib/${CPU_ARCHITECTURE}/dxcompiler.lib")

target_include_directories(Renderer PUBLIC
	""								# this gets the current directory
	"Vulkan"
	"../../resources/shaders/HLSL"	# for the shared shader headers
)

add_custom_command(TARGET Renderer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_SOURCE_DIR}/lib/DXC/bin/${CPU_ARCHITECTURE}/dxcompiler.dll"
        $<TARGET_FILE_DIR:Renderer>)

add_custom_command(TARGET Renderer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_SOURCE_DIR}/lib/DXC/bin/${CPU_ARCHITECTURE}/dxil.dll"
        $<TARGET_FILE_DIR:Renderer>)
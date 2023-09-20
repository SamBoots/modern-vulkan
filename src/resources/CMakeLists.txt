# CMakeList.txt : CMake project for Modern Vulkan, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

#copy json files when a change happened.
add_custom_target(copy_resources ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Copying unit test resources")

add_dependencies(Renderer copy_resources)
add_dependencies(image_filter_test copy_resources)
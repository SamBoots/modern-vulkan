﻿# CMakeList.txt : CMake project for Modern Vulkan, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

#copy shaders when a change happened.
add_custom_target(copy_shaders ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/resources/shaders
    ${CMAKE_BINARY_DIR}/resources/shaders
    COMMENT "Copying Shaders")

add_dependencies(Renderer copy_shaders)

#copy models when a change happened.
add_custom_target(copy_models ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/resources/models
    ${CMAKE_BINARY_DIR}/resources/models
    COMMENT "Copying models")

add_dependencies(Renderer copy_models)

#copy models when a change happened.
add_custom_target(copy_textures ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/resources/textures
    ${CMAKE_BINARY_DIR}/resources/textures
    COMMENT "Copying Textures")

add_dependencies(Renderer copy_textures)
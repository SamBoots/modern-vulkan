﻿# CMakeList.txt : CMake project for Modern Vulkan, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

#copy models when a change happened.
add_custom_target(copy_models ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/resources/models
    ${CMAKE_BINARY_DIR}/resources/models
    COMMENT "Copying models")

add_dependencies(Engine copy_models)

#copy textures when a change happened.
add_custom_target(copy_textures ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/resources/textures
    ${CMAKE_BINARY_DIR}/resources/textures
    COMMENT "Copying Textures")

add_dependencies(Engine copy_textures)

#copy models when a change happened.
add_custom_target(copy_lua ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/resources/lua
    ${CMAKE_BINARY_DIR}/resources/lua
    COMMENT "Copying lua directory")

add_dependencies(Game copy_lua)

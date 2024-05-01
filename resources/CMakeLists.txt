# CMakeList.txt : CMake project for Modern Vulkan, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

#copy shaders when a change happened.
add_custom_target(copy_shaders ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/resources/shaders
    ${CMAKE_BINARY_DIR}/resources/shaders
    COMMENT "Copying Shaders")

add_dependencies(Engine copy_shaders)

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

#copy scenes when a change happened.
add_custom_target(copy_scenes ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/resources/scenes
    ${CMAKE_BINARY_DIR}/resources/scenes
    COMMENT "Copying scenes")

add_dependencies(Engine copy_scenes)

#copy models when a change happened.
add_custom_target(copy_game_res ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/resources/game
    ${CMAKE_BINARY_DIR}/resources/game
    COMMENT "Copying game directory")

add_dependencies(Game copy_game_res)

#copy models when a change happened.
add_custom_target(copy_filter_images ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/resources/filter_textures
    ${CMAKE_BINARY_DIR}/resources/filter_textures
    COMMENT "Copying image filter directory")

add_dependencies(image_filter_test copy_filter_images)

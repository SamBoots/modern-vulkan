###############################################################
#  This cmakelist handles the Engine setup for Modern Vulkan  #
###############################################################
cmake_minimum_required (VERSION 3.8)

add_library(Engine
	"ecs/EntityMap.cpp"
	"ecs/EntityComponentSystem.cpp"
	"ecs/components/RenderComponent.cpp"
	"ecs/components/LightComponent.cpp"
	"ecs/components/NameComponent.cpp"
    "ecs/components/RaytraceComponent.cpp"
	"ecs/systems/RenderSystem.cpp"
    "ecs/systems/ClearStage.cpp"
    "ecs/systems/ShadowMapStage.cpp"
    "ecs/systems/RasterMeshStage.cpp"
    "ecs/systems/BloomStage.cpp"
    "ecs/systems/LineStage.cpp"
    "lua/LuaECSApi.cpp"
    "lua/LuaEngine.cpp"
    "lua/LuaTypes.cpp" 
    "lua/LuaTest.cpp"
	"AssetLoader.cpp"
	"SceneHierarchy.cpp"
	"MaterialSystem.cpp"
    "InputSystem.cpp"
	"EngineConfig.cpp"
	"Viewport.cpp"
	"Profiler.cpp"
	"ImGuiImpl.cpp"
    "Engine.cpp" 
    "GameInstance.cpp")
	
target_include_directories(Engine PUBLIC "")

target_link_libraries(Engine
	mikktspace
	cgltf
	stb_image
	Renderer)

# Modern Vulkan Game Engine
A game engine using modern vulkan techniques such as descriptor buffers, dynamic rendering and pipeline state-less rendering using VK_EXT_shader_object.

![image](https://github.com/user-attachments/assets/ba1c9cf9-fdf8-482c-9c08-d3e0dbcde859)

## Engine Standouts
### [BBFramework](https://github.com/SamBoots/BBFramework/tree/main), my custom framework to replace most of the STL 
My own framework to replace parts of the STL to learn how and why it works. At times adjusting it to exactly what I need. 
Most data containers and cross-platform code comes from here. The base bones of the project.
This project has a modified version of the BBFramework and is consindered to be the most up to date.
### Tight memory control
Most if not all memory is managed via memory arena's, which are linear allocators with debug functionality to track memory usage.
There are memory interfaces for freelists, but the underlying memory comes from a linear allocator.
### Multiple scenes with async updating
The engine is made to support multiple scenes. With the image above you can see the the showcase rendering scene, and a game scene with a generated "dungeon" layout.
These scenes can be updated async and you can have multiple of these scenes.
### Custom math library
In order to learn the underlying math I have implemented most math functions and primitives myself. Pure for learning.
All scenes handle their own input events. 

### ECS - WIP
The engine's internals will be using an ECS system per scene. This will simplify the scene rendering a whole lot.

## All features
#### Storage Containers
- Dynamic Array (Vector)
- Freelist Array
- Static Array
- String
- Stack String
- String View
- Slotmap
- Unordered hashmap
- Open Addressing Hashmap
- Queue
- Multiple Producer Single Consumer Queue
- Sparseset
#### Rendering Low level
- Vulkan
- Multiple Aync Commandlist rendering
- Descriptor Buffers over Descriptor Sets
- Shader Objects over Render Pipelines
- Shader hot reloading
- Multi queue for graphics or asset loading calls.
#### Rendering High level
- Forward Rendering
- Bloom
- Normal Maps
- Shadow Mapping
- Multiple lights & shadows
#### Editor
- Transform hierarchy editor
- Light editor
- Screenshot scene function
- Load scene via json file
#### Asset System
- Async asset loading
- Load gltf with multiple threads
- Load images via stb image
- Resize images via stb image resize
- Generate and store on disk icons for assets
#### Input
- Raw Input API from Windows for Mouse & Keyboard
#### Game
- Rudamentry old-school dungeon navigation controls
- Dungeon generation via .jpg

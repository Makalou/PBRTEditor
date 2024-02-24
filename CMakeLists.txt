cmake_minimum_required(VERSION 3.12)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")

execute_process(COMMAND ./vcpkg install assimap meshoptimizer WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg)

project(PBRTEditor LANGUAGES CXX C)

# Get the absolute path of the current source directory
get_filename_component(PROJECT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)
message(STATUS "Project source directory: ${PROJECT_SOURCE_DIR}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# For sanitizers
list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_SOURCE_DIR}/cmake")

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message (STATUS "Setting build type to 'Release' as none was specified.")
    set (CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
    set_property (CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release"
            "MinSizeRel" "RelWithDebInfo")
endif ()

find_package(Threads)
find_package(Vulkan REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(meshoptimizer CONFIG REQUIRED)

if(${Vulkan_FOUND})
    message("find vulkan version : " ${Vulkan_VERSION})
endif ()

if(MSVC)
    message("")
endif ()

if(APPLE)
    message("Build on Apple platform")
endif ()

# for external

set(BUILD_SHARED_LIBS OFF)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/ext)

include_directories(src/ext)
include_directories(src/ext/glm)
include_directories(src/ext/glfw/include)
include_directories(src/ext/tinyobjloader)
include_directories(src/ext/imgui/include)
include_directories(src/ext/stb)
include_directories(src/ext/vma)
include_directories(src/ext/spirv_reflect)

add_executable(editor_exe src/pbrt_scene_editor/main.cpp 
                          src/pbrt_scene_editor/viewer.cpp 
                          src/pbrt_scene_editor/window.cpp 
                          src/pbrt_scene_editor/window.h
                          src/pbrt_scene_editor/window_config.h
                          src/pbrt_scene_editor/VkBootstrap.cpp
                          src/pbrt_scene_editor/VkBootstrap.h
                          src/pbrt_scene_editor/VkBootstrapDispatch.h
                          src/pbrt_scene_editor/editorGUI.h
                          src/pbrt_scene_editor/editorGUI.cpp
                          src/pbrt_scene_editor/VulkanExtension.h
                          src/pbrt_scene_editor/AssetFileTree.cpp
        src/pbrt_scene_editor/sceneViewer.cpp
        src/pbrt_scene_editor/sceneGraphEditor.cpp
        src/pbrt_scene_editor/AssetManager.cpp
        src/pbrt_scene_editor/Insepctor.cpp
        src/pbrt_scene_editor/PBRTParser.h
        src/pbrt_scene_editor/PBRTParser.cpp
        src/pbrt_scene_editor/TokenParser.cpp
        src/pbrt_scene_editor/LoggerGUI.cpp
        src/pbrt_scene_editor/GlobalLogger.h
        src/pbrt_scene_editor/SceneBuilder.hpp
        src/pbrt_scene_editor/SceneBuilder.cpp
        src/pbrt_scene_editor/Insepctor.cpp
        src/pbrt_scene_editor/RenderScene.h
        src/pbrt_scene_editor/RenderScene.cpp
        src/pbrt_scene_editor/GPUPass.h
        src/pbrt_scene_editor/ShaderManager.cpp
        src/pbrt_scene_editor/ShaderManager.h
        src/pbrt_scene_editor/Singleton.h
        src/pbrt_scene_editor/GPUPass.cpp
        src/pbrt_scene_editor/VulkanExtension.cpp
        src/pbrt_scene_editor/ApplicationConfig.h
        src/pbrt_scene_editor/ThreadPool.h)

target_compile_definitions(editor_exe PRIVATE EDITOR_PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

target_link_libraries(editor_exe PRIVATE Vulkan::Vulkan)
target_link_libraries(editor_exe PRIVATE glfw)
target_link_libraries(editor_exe PRIVATE imgui)
target_link_libraries(editor_exe PRIVATE stb)
target_link_libraries(editor_exe PRIVATE vma)
target_link_libraries(editor_exe PRIVATE assimp::assimp)
target_link_libraries(editor_exe PRIVATE meshoptimizer::meshoptimizer)
target_link_libraries(editor_exe PRIVATE spirv_reflect)






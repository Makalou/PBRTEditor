cmake_minimum_required(VERSION 3.12)

project(PBRTEditor LANGUAGES CXX C)

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

include_directories(src/ext/glm)
include_directories(src/ext/glfw/include)
include_directories(src/ext/tinyobjloader)
include_directories(src/ext/imgui/include)

add_executable(editor_exe src/pbrt_scene_editor/main.cpp src/pbrt_scene_editor/viewer.cpp)
target_link_libraries(editor_exe PRIVATE Vulkan::Vulkan)
target_link_libraries(editor_exe PRIVATE glfw)
target_link_libraries(editor_exe PRIVATE imgui)




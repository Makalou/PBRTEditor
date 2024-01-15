cmake_minimum_required(VERSION 3.20)
# Change template to your own project name
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

project(vma)

add_library(vma STATIC vma_impl.cpp)

find_package(Vulkan REQUIRED)
if (NOT ${Vulkan_INCLUDE_DIRS} STREQUAL "")
    #set(VULKAN_PATH ${Vulkan_INCLUDE_DIRS})
    #STRING(REGEX REPLACE "/Include" "" VULKAN_PATH ${VULKAN_PATH})
    message(STATUS "Successfully located the Vulkan SDK at: ${Vulkan_INCLUDE_DIRS}")
    target_include_directories(vma PUBLIC ${Vulkan_INCLUDE_DIRS})
endif()


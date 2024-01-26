cmake_minimum_required(VERSION 3.20)
# Change template to your own project name
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

project(spirv_reflect)

add_library(spirv_reflect STATIC
        spirv_reflect.cpp
        spirv_reflect.c
)

#target_compile_definitions(spirv_reflect PUBLIC SPIRV_REFLECT_USE_SYSTEM_SPIRV_H)

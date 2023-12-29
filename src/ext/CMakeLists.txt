#build glfw
option(GLFW_BUILD_DOCS "" OFF)
option(GLFW_BUILD_TESTS "" OFF)
option(GLFW_BUILD_EXAMPLES "" OFF)
add_subdirectory(glfw)

add_subdirectory(imgui)

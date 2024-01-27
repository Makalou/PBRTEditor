#include "window.h"
#include "window_config.h"
#include <stdexcept>

Window::Window()
{
	width = WINDOW_WIDTH;
	height = WINDOW_HEIGHT;
	title = "PBRTv4 Editor";
}

void Window::init() 
{
	if (glfwInit() == GLFW_FALSE)
	{
		throw std::runtime_error("glfw init failed!");
	}
	
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	window = glfwCreateWindow(width,height,title.c_str(), nullptr, nullptr);
	glfwSetWindowUserPointer(window, this);

	glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
	glfwSetKeyCallback(window, keyCallback);
	glfwSetScrollCallback(window, scrollCallback);
	glfwSetMouseButtonCallback(window, mouseButtonCallback);
}

Window::~Window()
{
	glfwDestroyWindow(window);
	glfwTerminate();
}

void Window::resize(int width, int height)
{

}

bool Window::shouldClose()
{
	return glfwWindowShouldClose(window);
}

void Window::pollEvents()
{
	glfwPollEvents();
}

GLFWwindow* Window::getRawWindowHandle() const
{
	return window;
}

std::vector<framebufferResizeCallbackT> Window::framebufferResizeCallbackList = {};
std::vector<keyCallbackT> Window::keyCallbackList = {};
std::vector<scrollCallbackT> Window::scrollCallbackList = {};
std::vector<mouseButtonCallbackT> Window::mouseButtonCallbackList = {};

void Window::registerFramebufferResizeCallback(framebufferResizeCallbackT fn)
{
	framebufferResizeCallbackList.push_back(fn);
}
void Window::registerKeyCallback(keyCallbackT fn)
{
	keyCallbackList.push_back(fn);
}
void Window::registerScrollCallback(scrollCallbackT fn)
{
	scrollCallbackList.push_back(fn);
}
void Window::registerMouseButtonCallback(mouseButtonCallbackT fn)
{
	mouseButtonCallbackList.push_back(fn);
}

void Window::framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
	for (auto& callback : framebufferResizeCallbackList)
	{
		callback(width,height);
	}
}
void Window::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	for (auto& callback : keyCallbackList)
	{
		callback(key, scancode, action, mods);
	}
}
void Window::scrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
	for (auto& callback : scrollCallbackList)
	{
		callback(xoffset,yoffset);
	}
}
void Window::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
	for (auto& callback : mouseButtonCallbackList)
	{
		callback(button,action,mods);
	}
}
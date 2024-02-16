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
    glfwSetCursorPosCallback(window,cursorPosCallback);
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
std::vector<cursorPosCallbackT> Window::cursorPosCallbackList = {};
std::vector<mouseDragCallbackT> Window::mouseDragCallbackList = {};

void Window::registerFramebufferResizeCallback(const framebufferResizeCallbackT& fn)
{
	framebufferResizeCallbackList.push_back(fn);
}
void Window::registerKeyCallback(const keyCallbackT& fn)
{
	keyCallbackList.push_back(fn);
}
void Window::registerScrollCallback(const scrollCallbackT& fn)
{
	scrollCallbackList.push_back(fn);
}
void Window::registerMouseButtonCallback(const mouseButtonCallbackT& fn)
{
	mouseButtonCallbackList.push_back(fn);
}

void Window::registerCursorPosCallback(const cursorPosCallbackT& fn) {
    cursorPosCallbackList.emplace_back(fn);
}

void Window::registerMouseDragCallback(const mouseDragCallbackT& fn) {
    mouseDragCallbackList.emplace_back(fn);
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

void Window::cursorPosCallback(GLFWwindow *window, double xPos, double yPos) {
    for(auto & callback : cursorPosCallbackList)
    {
        callback(xPos,yPos);
    }

    static bool is_mouse_button_left_pressing = false;

    if(glfwGetMouseButton(window,GLFW_MOUSE_BUTTON_LEFT)==GLFW_PRESS)
    {
        static double last_x = 0.0;
        static double last_y = 0.0;

        if(is_mouse_button_left_pressing)
        {
            double xOffset = xPos - last_x;
            double yOffset = yPos - last_y;

            for(auto & callback : mouseDragCallbackList)
            {
                callback(GLFW_MOUSE_BUTTON_LEFT,xOffset,yOffset);
            }
        }else{
            is_mouse_button_left_pressing = true;
        }

        last_x = xPos;
        last_y = yPos;
    }else{
        is_mouse_button_left_pressing = false;
    }

    static bool is_mouse_button_right_pressing = false;

    if(glfwGetMouseButton(window,GLFW_MOUSE_BUTTON_RIGHT)==GLFW_PRESS)
    {
        static double last_x = 0.0;
        static double last_y = 0.0;

        if(is_mouse_button_right_pressing)
        {
            double xOffset = xPos - last_x;
            double yOffset = yPos - last_y;

            for(auto & callback : mouseDragCallbackList)
            {
                callback(GLFW_MOUSE_BUTTON_RIGHT,xOffset,yOffset);
            }
        }else{
            is_mouse_button_right_pressing = true;
        }

        last_x = xPos;
        last_y = yPos;
    }else{
        is_mouse_button_right_pressing = false;
    }

    static bool is_mouse_button_middle_pressing = false;

    if(glfwGetMouseButton(window,GLFW_MOUSE_BUTTON_MIDDLE)==GLFW_PRESS)
    {
        static double last_x = 0.0;
        static double last_y = 0.0;

        if(is_mouse_button_middle_pressing)
        {
            double xOffset = xPos - last_x;
            double yOffset = yPos - last_y;

            for(auto & callback : mouseDragCallbackList)
            {
                callback(GLFW_MOUSE_BUTTON_MIDDLE,xOffset,yOffset);
            }
        }else{
            is_mouse_button_middle_pressing = true;
        }

        last_x = xPos;
        last_y = yPos;
    }else{
        is_mouse_button_middle_pressing = false;
    }
}
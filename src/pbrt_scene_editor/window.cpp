#include "window.h"
#include "window_config.h"
#include <stdexcept>
#include "imgui.h"
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

rocket::signal<void(int width, int height)> Window::framebufferResizeSignal;
rocket::signal<void(int key, int scancode, int action, int mods)> Window::keySignal;
rocket::signal<void(double xoffset, double yoffset)> Window::scrollSignal;
rocket::signal<void(int button, int action, int mods)> Window::mouseButtonSignal;
rocket::signal<void(double xPos, double yPos)> Window::cursorPosSignal;
rocket::signal<void(int button, double xOffset, double yOffset)> Window::mouseDragSignal;

void Window::registerFramebufferResizeCallback(const framebufferResizeCallbackT& fn)
{
	framebufferResizeSignal.connect(fn);
}
void Window::registerKeyCallback(const keyCallbackT& fn)
{
    keySignal.connect(fn);
}
void Window::registerScrollCallback(const scrollCallbackT& fn)
{
    scrollSignal.connect(fn);
}
void Window::registerMouseButtonCallback(const mouseButtonCallbackT& fn)
{
    mouseButtonSignal.connect(fn);
}

void Window::registerCursorPosCallback(const cursorPosCallbackT& fn) {
    cursorPosSignal.connect(fn);
}

void Window::registerMouseDragCallback(const mouseDragCallbackT& fn) {
    mouseDragSignal.connect(fn);
}

void Window::framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
	framebufferResizeSignal(width,height);
}
void Window::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if(ImGui::GetIO().WantCaptureKeyboard)
    {
        return;
    }
	keySignal(key,scancode,action,mods);
}
void Window::scrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    if(ImGui::GetIO().WantCaptureMouse)
    {
        return;
    }
    scrollSignal(xoffset,yoffset);
}
void Window::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    if(ImGui::GetIO().WantCaptureMouse)
    {
        return;
    }
    mouseButtonSignal(button,action,mods);
}

void Window::cursorPosCallback(GLFWwindow *window, double xPos, double yPos) {
    if(ImGui::GetIO().WantCaptureMouse)
    {
        return;
    }
    cursorPosSignal(xPos,yPos);

    static bool is_mouse_button_left_pressing = false;

    if(glfwGetMouseButton(window,GLFW_MOUSE_BUTTON_LEFT)==GLFW_PRESS)
    {
        static double last_x = 0.0;
        static double last_y = 0.0;

        if(is_mouse_button_left_pressing)
        {
            double xOffset = xPos - last_x;
            double yOffset = yPos - last_y;

            /*for(auto & callback : mouseDragCallbackList)
            {
                callback(GLFW_MOUSE_BUTTON_LEFT,xOffset,yOffset);
            }*/
            mouseDragSignal(GLFW_MOUSE_BUTTON_LEFT,xOffset,yOffset);
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
            mouseDragSignal(GLFW_MOUSE_BUTTON_RIGHT,xOffset,yOffset);
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
            mouseDragSignal(GLFW_MOUSE_BUTTON_RIGHT,xOffset,yOffset);
        }else{
            is_mouse_button_middle_pressing = true;
        }

        last_x = xPos;
        last_y = yPos;
    }else{
        is_mouse_button_middle_pressing = false;
    }
}
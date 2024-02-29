#pragma once
#define GLFW_INCLUDE_VULKAN

#include <GLFW/glfw3.h>
#include <string>
#include <vector>
#include <functional>
#include "rocket.hpp"

using framebufferResizeCallbackT = std::function<void(int width, int height)>;
using keyCallbackT = std::function<void(int key, int scancode, int action, int mods)>;
using scrollCallbackT = std::function<void(double xoffset, double yoffset)>;
using mouseButtonCallbackT = std::function<void(int button, int action, int mods)>;
using cursorPosCallbackT = std::function<void(double xPos, double yPos)>;
using mouseDragCallbackT = std::function<void(int button, double xOffset, double yOffset)>;

struct Window {
public:
	Window();
	void init();
	~Window();
	bool shouldClose();
	void pollEvents();
	GLFWwindow* getRawWindowHandle() const;

    /*(width, height)*/
	static void registerFramebufferResizeCallback(const framebufferResizeCallbackT& fn);

    /*(key, scancode, action, mods)*/
	static void registerKeyCallback(const keyCallbackT& fn);

    /*(xoffset, yoffset)*/
	static void registerScrollCallback(const scrollCallbackT& fn);

    /*(button, action, modes)*/
	static void registerMouseButtonCallback(const mouseButtonCallbackT& fn);

    /*(xPos, yPos)*/
    static void registerCursorPosCallback(const cursorPosCallbackT& fn);

    /*(button, xOffset, yOffset)*/
    static void registerMouseDragCallback(const mouseDragCallbackT& fn);

private:
	GLFWwindow* window;
	int width, height;
	std::string title;

	static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
	static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
	static void scrollCallback(GLFWwindow* window,double xoffset, double yoffset);
	static void mouseButtonCallback(GLFWwindow* window,int button, int action, int mods);
    static void cursorPosCallback(GLFWwindow* window,double xPos, double yPos);

    static rocket::signal<void(int width, int height)> framebufferResizeSignal;
    static rocket::signal<void(int key, int scancode, int action, int mods)> keySignal;
    static rocket::signal<void(double xoffset, double yoffset)> scrollSignal;
    static rocket::signal<void(int button, int action, int mods)> mouseButtonSignal;
    static rocket::signal<void(double xPos, double yPos)> cursorPosSignal;
    static rocket::signal<void(int button, double xOffset, double yOffset)> mouseDragSignal;
};

#pragma once
#define GLFW_INCLUDE_VULKAN

#include <GLFW/glfw3.h>
#include <string>
#include <vector>
#include <functional>

using framebufferResizeCallbackT = std::function<void(int width, int height)>;
using keyCallbackT = std::function<void(int key, int scancode, int action, int mods)>;
using scrollCallbackT = std::function<void(double xoffset, double yoffset)>;
using mouseButtonCallbackT = std::function<void(int button, int action, int mods)>;
using cursorPosCallbackT = std::function<void(double xPos, double yPos)>;

struct Window {
public:
	Window();
	void init();
	~Window();
	void resize(int width, int height);
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

private:
	GLFWwindow* window;
	int width, height;
	std::string title;

	static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
	static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
	static void scrollCallback(GLFWwindow* window,double xoffset, double yoffset);
	static void mouseButtonCallback(GLFWwindow* window,int button, int action, int mods);
    static void cursorPosCallback(GLFWwindow* window,double xPos, double yPos);

	static std::vector<framebufferResizeCallbackT> framebufferResizeCallbackList;
	static std::vector<keyCallbackT> keyCallbackList;
	static std::vector<scrollCallbackT> scrollCallbackList;
	static std::vector<mouseButtonCallbackT> mouseButtonCallbackList;
    static std::vector<cursorPosCallbackT> cursorPosCallbackList;

};

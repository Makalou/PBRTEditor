#pragma once
#define GLFW_INCLUDE_VULKAN

#include <GLFW/glfw3.h>
#include <string>
#include <vector>
#include <functional>

using framebufferResizeCallbackT = std::function<void(int, int)>;
using keyCallbackT = std::function<void(int, int, int, int)>;
using scrollCallbackT = std::function<void(double, double)>;
using mouseButtonCallbackT = std::function<void(int, int, int)>;

struct Window {
public:
	Window();
	void init();
	~Window();
	void resize(int width, int height);
	bool shouldClose();
	void pollEvents();
	GLFWwindow* getRawWindowHandle() const;
	
	static void registryFramebufferResizeCallback(framebufferResizeCallbackT fn);
	static void registrykeyCallback(keyCallbackT fn);
	static void registryScrollCallback(scrollCallbackT fn);
	static void registryMouseButtonCallback(mouseButtonCallbackT fn);

private:
	GLFWwindow* window;
	int width, height;
	std::string title;

	static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
	static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
	static void scrollCallback(GLFWwindow* window,double xoffset, double yoffset);
	static void mouseButtonCallback(GLFWwindow* window,int button, int action, int mods);

	static std::vector<framebufferResizeCallbackT> framebufferResizeCallbackList;
	static std::vector<keyCallbackT> keyCallbackList;
	static std::vector<scrollCallbackT> scrollCallbackList;
	static std::vector<mouseButtonCallbackT> mouseButtonCallbackList;

};

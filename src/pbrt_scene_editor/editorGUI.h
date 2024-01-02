#pragma once

#include <vulkan/vulkan.hpp>

#include <memory>
#include <deque>
#include <filesystem>

#define MAX_FRAME_IN_FLIGHT 3

struct GLFWwindow;
struct DeviceExtended;
struct CommandPoolExtended;
struct SwapchainExtended;
struct EditorComponent;

struct Inspector;
struct AssetFileTree;
struct SceneViewer;
struct SceneGraphEditor;
struct LoggerGUI;

struct EditorGUI
{
public:
	EditorGUI();
	void init(GLFWwindow* window,std::shared_ptr<DeviceExtended> device);
	//Construct a new frame layout, collect all GUI data
	void constructFrame();
	//
	vk::CommandBuffer recordGraphicsCommand(unsigned int idx);
	~EditorGUI(); 

	//components
	Inspector* _inspector = nullptr;
	AssetFileTree* _assetFileTree = nullptr;
	SceneViewer* _sceneViewer = nullptr;
	SceneGraphEditor* _sceneGraphEditor = nullptr;
    LoggerGUI* _loggerWindow = nullptr;

private:
	std::deque < std::pair<std::string, std::filesystem::path>> recentOpenCache;
	void showMainMenuBar();
	void showMenuFile();
	void showMenuEdit();
	void showMenuView();
	void showMenuRender();
	void showMenuTools();
	void render(vk::CommandBuffer, unsigned int idx);
	void createVulkanResource();
	void createRenderPass(SwapchainExtended* swapchain);
	void createGuiFrameBuffer(SwapchainExtended* swapchain);
	void destroyVulkanResource();

	std::shared_ptr<DeviceExtended> backendDevice;
	std::vector<CommandPoolExtended> commandPools;
	std::vector<vk::CommandBuffer> commandBuffers;
	std::vector<vk::Framebuffer> frameBuffers;
	
	vk::RenderPass guiPass;
	vk::DescriptorPool descriptorPool;

	bool fileSelectorOpen = false;
};
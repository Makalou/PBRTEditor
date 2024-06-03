#pragma once

#include <vulkan/vulkan.hpp>

#include <memory>
#include <deque>
#include <filesystem>

#define NUM_MIN_SWAPCHAIN_IMAGE 3

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
struct OfflineRenderGUI;

struct FrameGraph;

struct EditorGUI
{
public:
	EditorGUI();
	void init(GLFWwindow* window,std::shared_ptr<DeviceExtended> device);
	//Construct a new frame layout, collect all GUI data
	void constructFrame();
	~EditorGUI(); 

    SceneViewer* viewer;

	//components
	Inspector* _inspector = nullptr;
	AssetFileTree* _assetFileTree = nullptr;
	SceneGraphEditor* _sceneGraphEditor = nullptr;
    LoggerGUI* _loggerWindow = nullptr;
	OfflineRenderGUI* _offlineRender = nullptr;
	std::filesystem::path currentPBRTSceneFilePath;

	void constructFrameGraphAOT(FrameGraph* frameGraph);

	void renderInit(vk::RenderPass);
	void render(vk::CommandBuffer);

private:
	std::deque < std::pair<std::string, std::filesystem::path>> recentOpenCache;
	void showMainMenuBar();
	void showMenuFile();
	void showMenuEdit();
	void showMenuShading();
	void showMenuView();
	void showMenuRender();
	void showMenuTools();
	void createVulkanResource();
	void destroyVulkanResource();

	std::shared_ptr<DeviceExtended> backendDevice;
	
	vk::DescriptorPool descriptorPool;

	bool fileSelectorOpen = false;
};
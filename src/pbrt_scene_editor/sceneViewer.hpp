#pragma once

#include "vulkan/vulkan.hpp"
#include <memory>
#include "RenderScene.h"
#include "GPUPass.h"

struct DeviceExtended;
struct SceneGraphNode;

struct SceneViewerCamera
{

};

struct SceneViewer
{
    void constructFrame();
	void init(std::shared_ptr<DeviceExtended> device);
    void setCurrentSceneGraph(SceneGraphNode* root,AssetManager& assetManager);
    vk::CommandBuffer recordGraphicsCommand(unsigned int idx);
	~SceneViewer();

    enum class RenderingMode
    {
        RASTERIZATION,
        RAYTRACING
    };

private:
	std::shared_ptr<DeviceExtended> backendDevice;
    SceneGraphNode* _currentSceneGraph;
    RenderingMode _renderingMode;
    renderScene::RenderScene* _renderScene;
    std::vector<GPUFrame> _gpuFrames;
};
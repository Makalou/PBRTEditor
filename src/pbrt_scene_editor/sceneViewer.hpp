#pragma once

#include "vulkan/vulkan.hpp"
#include <memory>
#include "RenderScene.h"
#include "GPUPass.h"
#include "GPUFrame.hpp"

struct DeviceExtended;
struct SceneGraphNode;

struct SceneViewerCamera
{

};

struct SceneViewer
{
    void constructFrame();
    void constructFrameGraphAOT(FrameGraph* frameGraph);
	void init(std::shared_ptr<DeviceExtended> device);
    void setCurrentSceneGraph(SceneGraph* sceneGraph,AssetManager& assetManager);
    void update(FrameGraph* frameGraph);

	~SceneViewer();

    enum class RenderingMode
    {
        RASTERIZATION,
        RAYTRACING
    };

    enum class ShadingMode
    {
        FLAT,
        MESHID,
        POSITION,
        NORMAL,
        UV,
        ALBEDO,
        FINAL
    };

    ShadingMode currenShadingMode = ShadingMode::ALBEDO;
    bool enableWireFrame = false;

private:
	std::shared_ptr<DeviceExtended> backendDevice;
    RenderingMode _renderingMode;
    renderScene::RenderScene* _renderScene;
    std::vector<GPUFrame> _gpuFrames;
};
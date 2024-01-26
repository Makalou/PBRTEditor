#include "sceneViewer.hpp"

#include <utility>

#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "imgui_impl_glfw.h"
#include "implot.h"

#include "VulkanExtension.h"
#include "RenderScene.h"
#include "sceneGraphEditor.hpp"
#include "PassDefinition.h"

void SceneViewer::constructFrame()
{

}

void SceneViewer::init(std::shared_ptr<DeviceExtended> device) {
    backendDevice = std::move(device);
    _renderScene = new renderScene::RenderScene;
    _renderScene->backendDevice = backendDevice;

    GPUFrame frameGraph(8,backendDevice);
    auto skyBoxPass = std::make_shared<SkyBoxPass>();
    skyBoxPass->addOutput(std::make_unique<PassAttachmentDescription>("result",vk::Format::eR8G8B8A8Srgb,800,600,
                                                                      vk::AttachmentLoadOp::eDontCare,vk::AttachmentStoreOp::eStore));
    frameGraph.registerRasterizedGPUPass(skyBoxPass);

    auto gBufferPass = std::make_shared<GBufferPass>();
    gBufferPass->addOutput(std::make_unique<PassAttachmentDescription>("depth",vk::Format::eD32Sfloat,800,600,
                                                                       vk::AttachmentLoadOp::eDontCare,vk::AttachmentStoreOp::eStore));
    gBufferPass->addOutput(std::make_unique<PassAttachmentDescription>("wPosition",vk::Format::eR16G16B16Sfloat,800,600,
                                                                       vk::AttachmentLoadOp::eDontCare,vk::AttachmentStoreOp::eStore));
    gBufferPass->addOutput(std::make_unique<PassAttachmentDescription>("wNormal",vk::Format::eR16G16B16Sfloat,800,600,
                                                                       vk::AttachmentLoadOp::eDontCare,vk::AttachmentStoreOp::eStore));
    frameGraph.registerRasterizedGPUPass(gBufferPass);

    auto shadowPass = std::make_shared<ShadowPass>();
    shadowPass->addOutput(std::make_unique<PassAttachmentDescription>("mainShadowMap",vk::Format::eD32Sfloat,800,600,
                                                                      vk::AttachmentLoadOp::eDontCare,vk::AttachmentStoreOp::eStore));
    frameGraph.registerRasterizedGPUPass(shadowPass);

    auto deferredLightingPass = std::make_shared<DeferredLightingPass>();
    deferredLightingPass->addInput(std::make_unique<PassAttachmentDescription>("SkyBoxPass::result",
                                                                               vk::AttachmentLoadOp::eDontCare,vk::AttachmentStoreOp::eStore));
    deferredLightingPass->addInput(std::make_unique<PassTextureDescription>("GBufferPass::wPosition"));
    deferredLightingPass->addInput(std::make_unique<PassTextureDescription>("GBufferPass::wNormal"));
    deferredLightingPass->addInput(std::make_unique<PassTextureDescription>("ShadowPass::mainShadowMap"));
    deferredLightingPass->addOutput(std::make_unique<PassAttachmentDescription>("result",vk::AttachmentLoadOp::eDontCare,vk::AttachmentStoreOp::eStore));
    frameGraph.registerRasterizedGPUPass(deferredLightingPass);

    auto postProcessPass = std::make_shared<PostProcessPass>();
    postProcessPass->addInput(std::make_unique<PassAttachmentDescription>("SwapchainImage",vk::AttachmentLoadOp::eDontCare,vk::AttachmentStoreOp::eStore));
    postProcessPass->addInput(std::make_unique<PassTextureDescription>("DeferredLightingPass::result"));
    postProcessPass->addOutput(std::make_unique<PassAttachmentDescription>("result",vk::AttachmentLoadOp::eDontCare,vk::AttachmentStoreOp::eStore));
    frameGraph.registerRasterizedGPUPass(postProcessPass);

    for(int i = 0 ; i < 3; i ++)
    {
        _gpuFrames.push_back(frameGraph);
    }

    for(int i = 0 ; i < 3; i ++)
    {
        _gpuFrames[i].frameIdx = i;
        _gpuFrames[i].compileAOT();
    }

}

void SceneViewer::setCurrentSceneGraph(SceneGraphNode *root,AssetManager& assetManager)
{
    _currentSceneGraph = root;
    //construct render scene (ECS)
    _renderScene->buildFrom(root,assetManager);
}

vk::CommandBuffer SceneViewer::recordGraphicsCommand(unsigned int idx) {
    return _gpuFrames[idx].recordMainQueueCommands();
}

SceneViewer::~SceneViewer() = default;

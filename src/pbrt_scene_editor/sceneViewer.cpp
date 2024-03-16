#include "sceneViewer.hpp"

#include <utility>

#include "ApplicationConfig.h"

#include "VulkanExtension.h"
#include "RenderScene.h"
#include "sceneGraphEditor.hpp"
#include "PassDefinition.h"

void SceneViewer::constructFrame()
{

}

void SceneViewer::init(std::shared_ptr<DeviceExtended> device) {
    backendDevice = std::move(device);
    _renderScene = new renderScene::RenderScene(backendDevice);

    for(int i = 0 ; i < 3; i ++)
    {
        GPUFrame frameGraph(8,backendDevice);
        {
            auto skyBoxPass = std::make_unique<SkyBoxPass>();
            skyBoxPass->addOutput<PassAttachmentDescription>("result",vk::Format::eR8G8B8A8Srgb,PassAttachmentExtent::SwapchainRelative(1.0,1.0),
                                                             vk::AttachmentLoadOp::eDontCare,vk::AttachmentStoreOp::eStore);
            skyBoxPass->scene = this->_renderScene;
            frameGraph.registerRasterizedGPUPass(std::move(skyBoxPass));
        }
        {
            auto gBufferPass = std::make_unique<GBufferPass>();
            gBufferPass->scene = this->_renderScene;
            gBufferPass->addOutput<PassAttachmentDescription>("depth",vk::Format::eD32Sfloat, PassAttachmentExtent::SwapchainRelative(1.0, 1.0),
                                                              vk::AttachmentLoadOp::eClear,vk::AttachmentStoreOp::eStore);
            gBufferPass->addOutput<PassAttachmentDescription>("wPosition",vk::Format::eR8G8B8A8Srgb, PassAttachmentExtent::SwapchainRelative(1.0, 1.0),
                                                              vk::AttachmentLoadOp::eClear,vk::AttachmentStoreOp::eStore);
            gBufferPass->addOutput<PassAttachmentDescription>("wNormal", vk::Format::eR8G8B8A8Srgb, PassAttachmentExtent::SwapchainRelative(1.0, 1.0),
                                                                vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore);
            gBufferPass->addOutput<PassAttachmentDescription>("UV", vk::Format::eR8G8B8A8Srgb, PassAttachmentExtent::SwapchainRelative(1.0, 1.0),
                                                                vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore);
            gBufferPass->addOutput<PassAttachmentDescription>("AlbedoColor", vk::Format::eR8G8B8A8Srgb, PassAttachmentExtent::SwapchainRelative(1.0, 1.0),
                                                              vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore);
            frameGraph.registerRasterizedGPUPass(std::move(gBufferPass));
        }
//        {
//            auto shadowPass = std::make_unique<ShadowPass>();
//            shadowPass->addOutput<PassAttachmentDescription>("mainShadowMap",vk::Format::eD32Sfloat,WINDOW_WIDTH,WINDOW_HEIGHT,
//                                                             vk::AttachmentLoadOp::eDontCare,vk::AttachmentStoreOp::eStore);
//            frameGraph.registerRasterizedGPUPass(std::move(shadowPass));
//        }
        {
            auto deferredLightingPass = std::make_unique<DeferredLightingPass>();
            deferredLightingPass->addInput<PassTextureDescription>("GBufferPass::wPosition");
            deferredLightingPass->addInput<PassTextureDescription>("GBufferPass::wNormal");
            deferredLightingPass->addInput<PassTextureDescription>("GBufferPass::AlbedoColor");
//            deferredLightingPass->addInput<PassTextureDescription>("ShadowPass::mainShadowMap");
            deferredLightingPass->addInOut<PassAttachmentDescription>("SkyBoxPass::result", "result", vk::AttachmentLoadOp::eLoad, vk::AttachmentStoreOp::eStore);
            frameGraph.registerRasterizedGPUPass(std::move(deferredLightingPass));
        }
        {
            auto postProcessPass = std::make_unique<PostProcessPass>();
            postProcessPass->addInput<PassTextureDescription>("DeferredLightingPass::result");
            postProcessPass->addInOut<PassAttachmentDescription>("SwapchainImage", "result", vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore);
            frameGraph.registerRasterizedGPUPass(std::move(postProcessPass));
        }
        {
            auto copyPass = std::make_unique<CopyPass>();
            copyPass->enabled = false;
            copyPass->addInput<PassTextureDescription>("GBufferPass::wPosition");
            copyPass->addInput<PassTextureDescription>("GBufferPass::wNormal");
            copyPass->addInput<PassTextureDescription>("GBufferPass::UV");
            copyPass->addInput<PassTextureDescription>("GBufferPass::AlbedoColor");
            copyPass->addInOut<PassAttachmentDescription>("SwapchainImage", "result", vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore);
            frameGraph.registerRasterizedGPUPass(std::move(copyPass));
        }
        _gpuFrames.push_back(std::move(frameGraph));
    }

    for(int i = 0 ; i < 3; i ++)
    {
        _gpuFrames[i].frameIdx = i;
        _gpuFrames[i].compileAOT();
    }
}

void SceneViewer::setCurrentSceneGraph(SceneGraph *graph,AssetManager& assetManager)
{
    //construct render scene (ECS)
    _renderScene->buildFrom(graph,assetManager);
}

vk::CommandBuffer SceneViewer::recordGraphicsCommand(unsigned int idx) {
    auto* copyPass = dynamic_cast<CopyPass*>(_gpuFrames[idx].getPass("CopyPass"));
    switch (currenShadingMode)
    {
    case SceneViewer::ShadingMode::FLAT:
        _gpuFrames[idx].getPass("DeferredLightingPass")->enabled = false;
        _gpuFrames[idx].getPass("PostProcessPass")->enabled = false;
        copyPass->enabled = true;
        copyPass->currentTexIdx.x = 0;
        break;
    case SceneViewer::ShadingMode::POSITION:
        _gpuFrames[idx].getPass("DeferredLightingPass")->enabled = false;
        _gpuFrames[idx].getPass("PostProcessPass")->enabled = false;
        copyPass->enabled = true;
        copyPass->currentTexIdx.x = 0;
        break;
    case SceneViewer::ShadingMode::NORMAL:
        _gpuFrames[idx].getPass("DeferredLightingPass")->enabled = false;
        _gpuFrames[idx].getPass("PostProcessPass")->enabled = false;
        copyPass->enabled = true;
        copyPass->currentTexIdx.x = 1;
        break;
    case SceneViewer::ShadingMode::UV:
        _gpuFrames[idx].getPass("DeferredLightingPass")->enabled = false;
        _gpuFrames[idx].getPass("PostProcessPass")->enabled = false;
        copyPass->enabled = true;
        copyPass->currentTexIdx.x = 2;
        break;
    case SceneViewer::ShadingMode::ALBEDO:
        _gpuFrames[idx].getPass("DeferredLightingPass")->enabled = false;
        _gpuFrames[idx].getPass("PostProcessPass")->enabled = false;
        copyPass->enabled = true;
        copyPass->currentTexIdx.x = 3;
        break;
    case SceneViewer::ShadingMode::FINAL:
        _gpuFrames[idx].getPass("DeferredLightingPass")->enabled = true;
        _gpuFrames[idx].getPass("PostProcessPass")->enabled = true;
        copyPass->enabled = false;
        break;
    default:
        break;
    }
    _renderScene->update();
    return _gpuFrames[idx].recordMainQueueCommands();
}

SceneViewer::~SceneViewer() = default;

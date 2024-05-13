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
            gBufferPass->addOutput<PassAttachmentDescription>("flat", vk::Format::eR8G8B8A8Srgb, PassAttachmentExtent::SwapchainRelative(1.0, 1.0),
                                                              vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore);
            gBufferPass->addOutput<PassAttachmentDescription>("meshID", vk::Format::eR8G8B8A8Srgb, PassAttachmentExtent::SwapchainRelative(1.0, 1.0),
                                                              vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore);
            gBufferPass->addOutput<PassAttachmentDescription>("wPosition",vk::Format::eR8G8B8A8Srgb, PassAttachmentExtent::SwapchainRelative(1.0, 1.0),
                                                              vk::AttachmentLoadOp::eClear,vk::AttachmentStoreOp::eStore);
            gBufferPass->addOutput<PassAttachmentDescription>("wNormal", vk::Format::eR8G8B8A8Srgb, PassAttachmentExtent::SwapchainRelative(1.0, 1.0),
                                                              vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore);
            gBufferPass->addOutput<PassAttachmentDescription>("UV", vk::Format::eR8G8B8A8Srgb, PassAttachmentExtent::SwapchainRelative(1.0, 1.0),
                                                              vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore);
            gBufferPass->addOutput<PassAttachmentDescription>("albedoColor", vk::Format::eR8G8B8A8Srgb, PassAttachmentExtent::SwapchainRelative(1.0, 1.0),
                                                              vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore);
            gBufferPass->addOutput<PassAttachmentDescription>("encodedMeshID", vk::Format::eR8G8B8A8Unorm, PassAttachmentExtent::SwapchainRelative(1.0, 1.0),
                                                              vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore);
            gBufferPass->addOutput<PassAttachmentDescription>("encodedInstanceID", vk::Format::eR8G8B8A8Unorm, PassAttachmentExtent::SwapchainRelative(1.0, 1.0),
                                                              vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore);
            frameGraph.registerRasterizedGPUPass(std::move(gBufferPass));
        }
        {
            auto selectedMaskPass = std::make_unique<SelectedMaskPass>();
            selectedMaskPass->scene = this->_renderScene;
            selectedMaskPass->addOutput<PassAttachmentDescription>("mask",vk::Format::eR8G8B8A8Srgb, PassAttachmentExtent::SwapchainRelative(1.0, 1.0),
                                                                   vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore);
            frameGraph.registerRasterizedGPUPass(std::move(selectedMaskPass));
        }
        {
            auto objectPickPass = std::make_unique<ObjectPickPass>();
            objectPickPass->scene = this->_renderScene;
            objectPickPass->addInput<PassTextureDescription>("GBufferPass::encodedMeshID");
            objectPickPass->addInput<PassTextureDescription>("GBufferPass::encodedInstanceID");
            frameGraph.registerComputeGPUPass(std::move(objectPickPass));
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
            deferredLightingPass->addInput<PassTextureDescription>("GBufferPass::albedoColor");
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
            copyPass->addInput<PassTextureDescription>("GBufferPass::flat");
            copyPass->addInput<PassTextureDescription>("GBufferPass::meshID");
            copyPass->addInput<PassTextureDescription>("GBufferPass::wPosition");
            copyPass->addInput<PassTextureDescription>("GBufferPass::wNormal");
            copyPass->addInput<PassTextureDescription>("GBufferPass::UV");
            copyPass->addInput<PassTextureDescription>("GBufferPass::albedoColor");
            copyPass->addInOut<PassAttachmentDescription>("PostProcessPass::result", "result", vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore);
            frameGraph.registerRasterizedGPUPass(std::move(copyPass));
        }
        {
            auto wireFramePass = std::make_unique<WireFramePass>();
            wireFramePass->scene = this->_renderScene;
            wireFramePass->addInOut<PassAttachmentDescription>("GBufferPass::depth", "depth", vk::AttachmentLoadOp::eLoad, vk::AttachmentStoreOp::eStore);
            wireFramePass->addInOut<PassAttachmentDescription>("CopyPass::result", "result", vk::AttachmentLoadOp::eLoad, vk::AttachmentStoreOp::eStore);
            frameGraph.registerRasterizedGPUPass(std::move(wireFramePass));
        }
        {
            auto outlinePass = std::make_unique<OutlinePass>();
            outlinePass->addInput<PassTextureDescription>("SelectedMaskPass::mask");
            outlinePass->addInOut<PassAttachmentDescription>("WireFramePass::result", "result", vk::AttachmentLoadOp::eLoad, vk::AttachmentStoreOp::eStore);
            frameGraph.registerRasterizedGPUPass(std::move(outlinePass));
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
    case SceneViewer::ShadingMode::MESHID:
        _gpuFrames[idx].getPass("DeferredLightingPass")->enabled = false;
        _gpuFrames[idx].getPass("PostProcessPass")->enabled = false;
        copyPass->enabled = true;
        copyPass->currentTexIdx.x = 1;
        break;
    case SceneViewer::ShadingMode::POSITION:
        _gpuFrames[idx].getPass("DeferredLightingPass")->enabled = false;
        _gpuFrames[idx].getPass("PostProcessPass")->enabled = false;
        copyPass->enabled = true;
        copyPass->currentTexIdx.x = 2;
        break;
    case SceneViewer::ShadingMode::NORMAL:
        _gpuFrames[idx].getPass("DeferredLightingPass")->enabled = false;
        _gpuFrames[idx].getPass("PostProcessPass")->enabled = false;
        copyPass->enabled = true;
        copyPass->currentTexIdx.x = 3;
        break;
    case SceneViewer::ShadingMode::UV:
        _gpuFrames[idx].getPass("DeferredLightingPass")->enabled = false;
        _gpuFrames[idx].getPass("PostProcessPass")->enabled = false;
        copyPass->enabled = true;
        copyPass->currentTexIdx.x = 4;
        break;
    case SceneViewer::ShadingMode::ALBEDO:
        _gpuFrames[idx].getPass("DeferredLightingPass")->enabled = false;
        _gpuFrames[idx].getPass("PostProcessPass")->enabled = false;
        copyPass->enabled = true;
        copyPass->currentTexIdx.x = 5;
        break;
    case SceneViewer::ShadingMode::FINAL:
        _gpuFrames[idx].getPass("DeferredLightingPass")->enabled = true;
        _gpuFrames[idx].getPass("PostProcessPass")->enabled = true;
        copyPass->enabled = false;
        break;
    default:
        break;
    }
    _gpuFrames[idx].getPass("WireFramePass")->enabled = enableWireFrame;
    _renderScene->update();
    return _gpuFrames[idx].recordMainQueueCommands();
}

SceneViewer::~SceneViewer() = default;

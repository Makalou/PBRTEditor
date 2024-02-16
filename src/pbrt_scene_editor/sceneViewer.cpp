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
            skyBoxPass->addOutput<PassAttachmentDescription>("result",vk::Format::eR8G8B8A8Srgb,WINDOW_WIDTH,WINDOW_HEIGHT,
                                                             vk::AttachmentLoadOp::eDontCare,vk::AttachmentStoreOp::eStore);
            frameGraph.registerRasterizedGPUPass(std::move(skyBoxPass));
        }
        {
            auto gBufferPass = std::make_unique<GBufferPass>();
            gBufferPass->scene = this->_renderScene;
//            gBufferPass->addOutput<PassAttachmentDescription>("depth",vk::Format::eD32Sfloat,WINDOW_WIDTH,WINDOW_HEIGHT,
//                                                              vk::AttachmentLoadOp::eClear,vk::AttachmentStoreOp::eStore);
            //gBufferPass->addOutput<PassAttachmentDescription>("wPosition",vk::Format::eR16G16B16A16Sfloat,WINDOW_WIDTH,WINDOW_HEIGHT,
            //                                                  vk::AttachmentLoadOp::eClear,vk::AttachmentStoreOp::eStore);
            gBufferPass->addOutput<PassAttachmentDescription>("wPosition",vk::Format::eR8G8B8A8Srgb,WINDOW_WIDTH,WINDOW_HEIGHT,
                                                              vk::AttachmentLoadOp::eClear,vk::AttachmentStoreOp::eStore);
//            gBufferPass->addOutput<PassAttachmentDescription>("wNormal",vk::Format::eR16G16B16A16Sfloat,WINDOW_WIDTH,WINDOW_HEIGHT,
//                                                              vk::AttachmentLoadOp::eClear,vk::AttachmentStoreOp::eStore);
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
            deferredLightingPass->addInput<PassAttachmentDescription>("SkyBoxPass::result",
                                                                      vk::AttachmentLoadOp::eLoad,vk::AttachmentStoreOp::eStore);
            deferredLightingPass->addInput<PassTextureDescription>("GBufferPass::wPosition");
            //deferredLightingPass->addInput<PassTextureDescription>("GBufferPass::wNormal");
//            deferredLightingPass->addInput<PassTextureDescription>("ShadowPass::mainShadowMap");
            deferredLightingPass->addOutput<PassAttachmentDescription>("result",vk::AttachmentLoadOp::eLoad,vk::AttachmentStoreOp::eStore);
            frameGraph.registerRasterizedGPUPass(std::move(deferredLightingPass));
        }
        {
            auto postProcessPass = std::make_unique<PostProcessPass>();
            postProcessPass->addInput<PassAttachmentDescription>("SwapchainImage",vk::AttachmentLoadOp::eClear,vk::AttachmentStoreOp::eStore);
            postProcessPass->addInput<PassTextureDescription>("DeferredLightingPass::result");
            postProcessPass->addOutput<PassAttachmentDescription>("result",vk::AttachmentLoadOp::eClear,vk::AttachmentStoreOp::eStore);
            frameGraph.registerRasterizedGPUPass(std::move(postProcessPass));
        }
        _gpuFrames.push_back(std::move(frameGraph));
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

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
        //https://app.diagrams.net/#G1oZrtZ4mJJVukgjC-h3H29EIItxIKzXKd#%7B%22pageId%22%3A%223b_IgyTFIOp2U2cZway7%22%7D
        auto skyBoxPass = std::make_unique<SkyBoxPass>();
        skyBoxPass->scene = this->_renderScene;
        auto gBufferPass = std::make_unique<GBufferPass>();
        gBufferPass->scene = this->_renderScene;
        auto selectedMaskPass = std::make_unique<SelectedMaskPass>();
        auto objectPickPass = std::make_unique<ObjectPickPass>();
        objectPickPass->scene = this->_renderScene;
        selectedMaskPass->scene = this->_renderScene;
        auto deferredLightingPass = std::make_unique<DeferredLightingPass>();
        auto postProcessPass = std::make_unique<PostProcessPass>();
        auto copyPass = std::make_unique<CopyPass>();
        auto wireFramePass = std::make_unique<WireFramePass>();
        wireFramePass->scene = this->_renderScene;
        auto outlinePass = std::make_unique<OutlinePass>();
        auto ssaoPass = std::make_unique<SSAOPass>();
        ssaoPass->scene = this->_renderScene;

        auto scene_selected = frameGraph.createBoolVariable("scene_selected", true);
        auto shading_mode = frameGraph.createSwitchVariable("shading_mode", "Albedo");
        auto enable_wireframe = frameGraph.createBoolVariable("enable_wireframe", false);

        auto selectedMask = frameGraph.createTexture("selectedMask", vk::Format::eR8G8B8A8Srgb);
        selectedMaskPass->renderTo(selectedMask, vk::AttachmentLoadOp::eClear);
        frameGraph.executeWhen(scene_selected, std::move(selectedMaskPass));

        auto tex1 = frameGraph.createTexture("tex1",vk::Format::eR8G8B8A8Srgb);
        skyBoxPass->renderTo(tex1);
        frameGraph.executeWhen(scene_selected, std::move(skyBoxPass));

        auto sceneDepth = frameGraph.createTexture("sceneDepth", vk::Format::eD32Sfloat);
        auto flat = frameGraph.createTexture("flat", vk::Format::eR8G8B8A8Srgb);
        auto meshID = frameGraph.createTexture("meshID", vk::Format::eR8G8B8A8Srgb);
        auto wPosition = frameGraph.createTexture("wPosition", vk::Format::eR8G8B8A8Srgb);
        auto wNormal = frameGraph.createTexture("wNormal", vk::Format::eR8G8B8A8Srgb);
        auto UV = frameGraph.createTexture("UV", vk::Format::eR8G8B8A8Srgb);
        auto albedoColor = frameGraph.createTexture("albedoColor", vk::Format::eR8G8B8A8Srgb);
        auto encodeMeshID = frameGraph.createTexture("encodeMeshID", vk::Format::eR8G8B8A8Unorm);
        auto encodeInstanceID = frameGraph.createTexture("encodeInstanceID", vk::Format::eR8G8B8A8Unorm);
 
        gBufferPass->renderTo(sceneDepth, vk::AttachmentLoadOp::eClear);
        gBufferPass->renderTo(flat, vk::AttachmentLoadOp::eClear);
        gBufferPass->renderTo(meshID, vk::AttachmentLoadOp::eClear);
        gBufferPass->renderTo(wPosition, vk::AttachmentLoadOp::eClear);
        gBufferPass->renderTo(wNormal, vk::AttachmentLoadOp::eClear);
        gBufferPass->renderTo(UV, vk::AttachmentLoadOp::eClear);
        gBufferPass->renderTo(albedoColor, vk::AttachmentLoadOp::eClear);
        gBufferPass->renderTo(encodeMeshID, vk::AttachmentLoadOp::eClear);
        gBufferPass->renderTo(encodeInstanceID,vk::AttachmentLoadOp::eClear);
        frameGraph.executeWhen(scene_selected, std::move(gBufferPass));

        /*auto ssaoMap = frameGraph.createTexture("ssaoMap", vk::Format::eR8G8B8A8Unorm,PassTextureExtent::SwapchainRelative(1.0,1.0));
        ssaoPass->sample(sceneDepth);
        ssaoPass->sample(wPosition);
        ssaoPass->sample(wNormal);
        ssaoPass->renderTo(ssaoMap, vk::AttachmentLoadOp::eClear);
        frameGraph.executePass(std::move(ssaoPass));*/

        objectPickPass->sample(encodeMeshID);
        objectPickPass->sample(encodeInstanceID);
        frameGraph.executeWhen(scene_selected, std::move(objectPickPass));
       
        deferredLightingPass->sample(wPosition);
        deferredLightingPass->sample(wNormal);
        deferredLightingPass->sample(albedoColor);
        deferredLightingPass->renderTo(tex1,vk::AttachmentLoadOp::eLoad);
        frameGraph.executeWhen(scene_selected & shading_mode.is("Final"), std::move(deferredLightingPass));

        postProcessPass->sample(tex1);
        postProcessPass->renderTo(frameGraph.getSwapchainTexture(), vk::AttachmentLoadOp::eClear);
        frameGraph.executeWhen(scene_selected & shading_mode.is("Final"), std::move(postProcessPass));
       
        copyPass->sample(flat);
        copyPass->sample(meshID);
        copyPass->sample(wPosition);
        copyPass->sample(wNormal);
        copyPass->sample(UV);
        copyPass->sample(albedoColor);
        copyPass->renderTo(frameGraph.getSwapchainTexture(),vk::AttachmentLoadOp::eClear);
        frameGraph.executeWhen(scene_selected & (!shading_mode.is("Final")), std::move(copyPass));
       
        wireFramePass->renderTo(sceneDepth, vk::AttachmentLoadOp::eLoad);
        wireFramePass->renderTo(frameGraph.getSwapchainTexture(), vk::AttachmentLoadOp::eLoad);
        frameGraph.executeWhen(scene_selected & enable_wireframe, std::move(wireFramePass));
       
        outlinePass->sample(selectedMask);
        outlinePass->renderTo(frameGraph.getSwapchainTexture(), vk::AttachmentLoadOp::eLoad);
        frameGraph.executeWhen(scene_selected, std::move(outlinePass));

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
            _gpuFrames[idx].setSwitchVariable("shading_mode", "Flat");
            copyPass->currentTexIdx.x = 0;
            break;
        case SceneViewer::ShadingMode::MESHID:
            _gpuFrames[idx].setSwitchVariable("shading_mode", "MeshID");
            copyPass->currentTexIdx.x = 1;
            break;
        case SceneViewer::ShadingMode::POSITION:
            _gpuFrames[idx].setSwitchVariable("shading_mode", "Position");
            copyPass->currentTexIdx.x = 2;
            break;
        case SceneViewer::ShadingMode::NORMAL:
            _gpuFrames[idx].setSwitchVariable("shading_mode", "Normal");
            copyPass->currentTexIdx.x = 3;
            break;
        case SceneViewer::ShadingMode::UV:
            _gpuFrames[idx].setSwitchVariable("shading_mode", "UV");
            copyPass->currentTexIdx.x = 4;
            break;
        case SceneViewer::ShadingMode::ALBEDO:
            _gpuFrames[idx].setSwitchVariable("shading_mode", "Albedo");
            copyPass->currentTexIdx.x = 5;
            break;
        case SceneViewer::ShadingMode::FINAL:
            _gpuFrames[idx].setSwitchVariable("shading_mode", "Final");
            break;
        default:
            break;
    }
    _gpuFrames[idx].setBoolVariable("enable_wireframe", enableWireFrame);
    _renderScene->update();
    return _gpuFrames[idx].recordMainQueueCommands();
}

SceneViewer::~SceneViewer() = default;

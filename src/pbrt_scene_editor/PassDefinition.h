//
// Created by 王泽远 on 2024/1/21.
//

#ifndef PBRTEDITOR_PASSDEFINITION_H
#define PBRTEDITOR_PASSDEFINITION_H

#include "GPUPass.h"
#include <map>

#define RASTERIZEDPASS_DEF_BEGIN(name) struct name : GPURasterizedPass { name() : GPURasterizedPass(#name){};

#define RASTERIZEDPASS_DEF_END(name) };

namespace FullScreenQuadDrawer
{
    auto static getVertexShader(DeviceExtended * device)
    {
        return ShaderManager::getInstance().createVertexShader(device,"fullScreenQuad.vert");
    }

    auto static getVertexInputStateInfo()
    {
        vk::PipelineVertexInputStateCreateInfo emptyVertexInputState{};
        emptyVertexInputState.setVertexBindingDescriptionCount(0);
        emptyVertexInputState.setVertexAttributeDescriptionCount(0);
        return emptyVertexInputState;
    }

    void static draw(vk::CommandBuffer commandBuffer)
    {
        commandBuffer.draw(3,1,0,0);
    }
};

RASTERIZEDPASS_DEF_BEGIN(SkyBoxPass)

    void prepareAOT(const GPUFrame* frame) override
    {
        auto vs = FullScreenQuadDrawer::getVertexShader(frame->backendDevice.get());
        auto fs = ShaderManager::getInstance().createFragmentShader(frame->backendDevice.get(),"proceduralSkyBox.frag");
        auto pipelineLayout = frame->backendDevice->createPipelineLayout2({frame->_frameGlobalDescriptorSetLayout});
        frame->backendDevice->setObjectDebugName(pipelineLayout,"SkyBoxPassPipelineLayout");

        VulkanGraphicsPipelineBuilder builder(frame->backendDevice->device,vs,fs,
                                              FullScreenQuadDrawer::getVertexInputStateInfo(),renderPass,
                                              pipelineLayout);
        auto pipeline = builder.build();
        frame->backendDevice->setObjectDebugName(pipeline.getPipeline(), "SkyBoxPassPipeline");
        this->graphicsPipelines.push_back(pipeline);
    }

    void record(vk::CommandBuffer cmdBuf, const GPUFrame* frame) override
    {
        beginPass(cmdBuf);
        cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics,graphicsPipelines[0].getPipeline());
        FullScreenQuadDrawer::draw(cmdBuf);
        endPass(cmdBuf);
    }
RASTERIZEDPASS_DEF_END(SkyBoxPass)

RASTERIZEDPASS_DEF_BEGIN(ShadowPass)
    void prepareAOT(const GPUFrame* frame) override
    {

    }

    void record(vk::CommandBuffer cmdBuf, const GPUFrame* frame) override
    {

    }
RASTERIZEDPASS_DEF_END(ShadowPass)

RASTERIZEDPASS_DEF_BEGIN(GBufferPass)
    vk::DescriptorPool descriptorPool;
    vk::DescriptorSet passDataDescriptorSet;
    vk::DescriptorSetLayout passDataDescriptorLayout;
    vk::PipelineLayout passLevelPipelineLayout;
    void prepareAOT(const GPUFrame* frame) override
    {
        std::vector<vk::DescriptorSetLayoutBinding> bindings;

        vk::DescriptorSetLayoutBinding camBinding;
        camBinding.setBinding(0);
        camBinding.setDescriptorType(vk::DescriptorType::eUniformBuffer);
        camBinding.setDescriptorCount(1);
        camBinding.setStageFlags(vk::ShaderStageFlagBits::eAllGraphics);
        bindings.push_back(camBinding);

        passDataDescriptorLayout = frame->backendDevice->createDescriptorSetLayout2(bindings);
        vk::DescriptorPoolCreateInfo descriptorPoolCreateInfo{};
        std::vector<vk::DescriptorPoolSize> poolSizes;
        poolSizes.reserve(bindings.size());
        for(const auto & binding : bindings)
        {
            poolSizes.emplace_back(binding.descriptorType,binding.descriptorCount);
        }
        descriptorPoolCreateInfo.setPoolSizes(poolSizes);
        descriptorPoolCreateInfo.setMaxSets(10);

        descriptorPool = frame->backendDevice->createDescriptorPool(descriptorPoolCreateInfo);
        passDataDescriptorSet = frame->backendDevice->allocateSingleDescriptorSet(descriptorPool,passDataDescriptorLayout);
        vk::WriteDescriptorSet write;
        write.setDstSet(passDataDescriptorSet);
        write.setDescriptorCount(1);
        write.setDescriptorType(vk::DescriptorType::eUniformBuffer);
        write.setDstBinding(0);
//        vk::DescriptorBufferInfo camBufferInfo{};
//        camBufferInfo.setBuffer(scene->mainView.camera.data[frame->frameIdx].getBuffer());
//        camBufferInfo.setRange(vk::WholeSize);
//        camBufferInfo.setOffset(0);
//        write.setBufferInfo(camBufferInfo);
//        frame->backendDevice->updateDescriptorSets(write,{});
        passLevelPipelineLayout = frame->backendDevice->createPipelineLayout2({frame->_frameGlobalDescriptorSetLayout,passDataDescriptorLayout});
    }

    using InstnaceUUIDMap = std::unordered_map<renderScene::InstanceUUID,int,renderScene::InstanceUUIDHash>;
    InstnaceUUIDMap pipelineMap;
    InstnaceUUIDMap instanceDescriptorSetMap;
    int currentPipelineIdx = -1;
    int currentInstanceDescriptorSetIdx = -1;
    std::vector<vk::DescriptorSet> instanceDescriptorSets;

    vk::DescriptorSetLayout getPerInstanceDescriptorSetLayout()
    {
        return {};
    }

    int getOrCreatePipeline(const GPUFrame* frame,const renderScene::InstanceBatchRigidDynamicType & instanceRigidDynamic)
    {
        // 1. Is there any existing compatible pipeline?
        // shaders(variant),
        // vertex input, pipelineLayout could be retrieved through shaders reflection
        // renderPass is determined by pass itself
        // We ‘build and cache' pipeline, according to its identifier
        auto vertexInputState = instanceRigidDynamic.getVertexInputState();
        ShaderManager::ShaderMacroList macroList;
        auto vertexAttr = instanceRigidDynamic.mesh->vertexAttribute;
        if(vertexAttr.normalOffset != -1)
        {
            macroList.emplace_back("HAS_VERTEX_NORMAL","1");
        }
        if(vertexAttr.tangentOffset != -1 && vertexAttr.biTangentOffset != -1)
        {
            macroList.emplace_back("HAS_VERTEX_TANGENT_AND_BITANGENT","1");
        }
        if(vertexAttr.uvOffset != -1)
        {
            macroList.emplace_back("HAS_VERTEX_UV","1");
        }

        auto vsUUID = ShaderManager::queryShaderVariantUUID("simple.vert",macroList);
        auto fsUUID = ShaderManager::queryShaderVariantUUID("simple.frag",macroList);

        int idx = -1;
        for(int i = 0; i< graphicsPipelines.size();i++)
        {
            if(graphicsPipelines[i].compatibleWith(vertexInputState) &&
               graphicsPipelines[i].compatibleWithVertexShader(vsUUID) &&
               graphicsPipelines[i].compatibleWithFragmentShader(fsUUID))
            {
                pipelineMap.emplace(instanceRigidDynamic.getUUID(),i);
                idx = i;
                break;
            }
        }
        if(idx == -1)
        {
            // Need to create new pipeline
//            vk::DescriptorSetLayout instanceDescriptorSetLayout;
//            auto pipelineLayout = frame->backendDevice->createPipelineLayout2({frame->_frameGlobalDescriptorSetLayout,
//                                                                               passDataDescriptorLayout,
//                                                                               instanceDescriptorSetLayout});
            auto* vs = ShaderManager::getInstance().createVertexShader(frame->backendDevice.get(),"simple.vert",macroList);
            auto* fs = ShaderManager::getInstance().createFragmentShader(frame->backendDevice.get(),"simple.frag",macroList);
            vk::PipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo{};
            VulkanGraphicsPipelineBuilder builder(frame->backendDevice->device,vs,fs,vertexInputState.getCreateInfo(),renderPass,passLevelPipelineLayout);
            auto pipeline = builder.build();
            graphicsPipelines.push_back(pipeline);
            idx = graphicsPipelines.size() - 1;
        }
        return idx;
    }

    void bindCurrentPipeline(vk::CommandBuffer cmdBuf, int pipelineIdx)
    {
        if(pipelineIdx!=currentPipelineIdx)
        {
            currentPipelineIdx = pipelineIdx;
            auto currentPipeline = graphicsPipelines[currentPipelineIdx];
            cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics,currentPipeline.getPipeline());
        }
    }

    void bindCurrentInstanceDescriptorSet(vk::CommandBuffer cmdBuf, int descriptorSetIdx)
    {
        if(descriptorSetIdx!=currentInstanceDescriptorSetIdx)
        {
            currentInstanceDescriptorSetIdx = descriptorSetIdx;
            auto descriptorSet = instanceDescriptorSets[currentInstanceDescriptorSetIdx];
            cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,graphicsPipelines[currentPipelineIdx].getLayout(),2,
                                      descriptorSet,nullptr);
        }
    }

    void bindRenderState(vk::CommandBuffer cmdBuf, const GPUFrame* frame, const renderScene::InstanceBatchRigidDynamicType & instanceRigidDynamic)
    {
        auto instUUID = instanceRigidDynamic.getUUID();
        auto pipelineIdx = pipelineMap.find(instUUID);
        if(pipelineIdx!=pipelineMap.end())
        {
            bindCurrentPipeline(cmdBuf,pipelineIdx->second);
        }else{
            auto idx = getOrCreatePipeline(frame,instanceRigidDynamic);
            bindCurrentPipeline(cmdBuf,idx);
        }
//        auto instanceDescriptorSetIdx = instanceDescriptorSetMap.find(instUUID);
//        if(instanceDescriptorSetIdx!=instanceDescriptorSetMap.end())
//        {
//            bindCurrentInstanceDescriptorSet(cmdBuf,instanceDescriptorSetIdx->second);
//        }else{
//            // Need to allocate new descriptorSet
//
//        }
    }

    void record(vk::CommandBuffer cmdBuf,const GPUFrame* frame) override
    {
        cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,passLevelPipelineLayout,1,passDataDescriptorSet,
                                  nullptr);
        beginPass(cmdBuf);

        if(scene!= nullptr)
        {
            for(int i = 0; i < scene->_dynamicRigidMeshBatch.size(); i++)
            {
                auto & instanceRigidDynamic = scene->_dynamicRigidMeshBatch[i];
                // renderState = vertex input state + pipeline layout + shader + pipeline
                // todo : Here is a big problem. The performance is unpredictable.
                bindRenderState(cmdBuf,frame,instanceRigidDynamic);
                //mesh instance know how to bind the geometry buffer, how to draw
                instanceRigidDynamic.drawAll(cmdBuf);
            }
        }
        endPass(cmdBuf);
    }

    renderScene::RenderScene* scene{};
    std::vector<std::pair<int,int>> pipelinesMap;
RASTERIZEDPASS_DEF_END(GBufferPass)

RASTERIZEDPASS_DEF_BEGIN(DeferredLightingPass)
    void prepareAOT(const GPUFrame* frame) override
    {
        auto vs = FullScreenQuadDrawer::getVertexShader(frame->backendDevice.get());
        auto fs = ShaderManager::getInstance().createFragmentShader(frame->backendDevice.get(),"deferred.frag");

        auto pipelineLayout = frame->backendDevice->createPipelineLayout2({frame->_frameGlobalDescriptorSetLayout});
        frame->backendDevice->setObjectDebugName(pipelineLayout, "DeferredLightingPassPipelineLayout");
        VulkanGraphicsPipelineBuilder builder(frame->backendDevice->device,vs,fs,
                                              FullScreenQuadDrawer::getVertexInputStateInfo(),renderPass,
                                              pipelineLayout);
        auto pipeline = builder.build();
        frame->backendDevice->setObjectDebugName(pipeline.getPipeline(), "DeferredLightingPassPipeline");
        graphicsPipelines.push_back(pipeline);
    }

    void record(vk::CommandBuffer cmdBuf,const GPUFrame* frame) override
    {
        beginPass(cmdBuf);
        //vk::Viewport viewport{};
        //cmdBuf.setViewport(0,viewport);
        //vk::Rect2D scissor{};
        //cmdBuf.setScissor(0,scissor);
        cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics,graphicsPipelines[0].getPipeline());
        FullScreenQuadDrawer::draw(cmdBuf);
        endPass(cmdBuf);
    }
RASTERIZEDPASS_DEF_END(DefereredLightingPass)

RASTERIZEDPASS_DEF_BEGIN(PostProcessPass)
    vk::DescriptorPool descriptorPool;
    vk::DescriptorSet passDataDescriptorSet;
    vk::PipelineLayout pipelineLayout;
    void prepareAOT(const GPUFrame* frame) override
    {
        auto baseLayout = frame->getPassDataDescriptorSetBaseLayout(this);
        auto passDataDescriptorLayout = frame->backendDevice->createDescriptorSetLayout2(baseLayout.bindings);
        frame->backendDevice->setObjectDebugName(passDataDescriptorLayout, "PostProcessPassDataDescriptorLayout");

        vk::DescriptorPoolCreateInfo descriptorPoolCreateInfo{};
        std::vector<vk::DescriptorPoolSize> poolSizes;
        poolSizes.reserve(baseLayout.bindings.size());
        for(const auto & binding : baseLayout.bindings)
        {
            poolSizes.emplace_back(binding.descriptorType,binding.descriptorCount);
        }
        descriptorPoolCreateInfo.setPoolSizes(poolSizes);
        descriptorPoolCreateInfo.setMaxSets(10);

        descriptorPool = frame->backendDevice->createDescriptorPool(descriptorPoolCreateInfo);
        passDataDescriptorSet = frame->backendDevice->allocateSingleDescriptorSet(descriptorPool,passDataDescriptorLayout);
        frame->backendDevice->setObjectDebugName(passDataDescriptorSet, "PostProcessPassDataDescriptorSet");

        for(auto & write : baseLayout.writes)
        {
            write.setDstSet(passDataDescriptorSet);
        }

        frame->backendDevice->updateDescriptorSets(baseLayout.writes,{});

        auto vs = FullScreenQuadDrawer::getVertexShader(frame->backendDevice.get());
        auto fs = ShaderManager::getInstance().createFragmentShader(frame->backendDevice.get(),"postProcess.frag");

        pipelineLayout = frame->backendDevice->createPipelineLayout2({frame->_frameGlobalDescriptorSetLayout,passDataDescriptorLayout});
        frame->backendDevice->setObjectDebugName(pipelineLayout, "PostProcessPassPipelineLayout");

        VulkanGraphicsPipelineBuilder builder(frame->backendDevice->device,vs,fs,
                                              FullScreenQuadDrawer::getVertexInputStateInfo(),renderPass,
                                              pipelineLayout);
        auto pipeline = builder.build();
        frame->backendDevice->setObjectDebugName(pipeline.getPipeline(), "PostProcessPassPipeline");
        graphicsPipelines.push_back(pipeline);
    }

    void record(vk::CommandBuffer cmdBuf,const GPUFrame* frame) override
    {
        beginPass(cmdBuf);
        //vk::Viewport viewport{};
        //cmdBuf.setViewport(0,viewport);
        //vk::Rect2D scissor{};
        //cmdBuf.setScissor(0,scissor);
        cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,pipelineLayout,1,passDataDescriptorSet, nullptr);
        cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics,graphicsPipelines[0].getPipeline());
        FullScreenQuadDrawer::draw(cmdBuf);
        endPass(cmdBuf);
    }

RASTERIZEDPASS_DEF_END(PostProcessPass)

#endif //PBRTEDITOR_PASSDEFINITION_H

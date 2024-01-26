//
// Created by 王泽远 on 2024/1/21.
//

#ifndef PBRTEDITOR_PASSDEFINITION_H
#define PBRTEDITOR_PASSDEFINITION_H

#include "GPUPass.h"

#define RASTERIZEDPASS_DEF_BEGIN(name) struct name : GPURasterizedPass { name() : GPURasterizedPass(#name){};

#define RASTERIZEDPASS_DEF_END(name) };

RASTERIZEDPASS_DEF_BEGIN(SkyBoxPass)
    void prepareAOT(const GPUFrame* frame) override
    {
        auto vs = ShaderManager::getInstance().createVertexShader(frame->backendDevice.get(),"fullScreenQuad.vert");
        auto fs = ShaderManager::getInstance().createFragmentShader(frame->backendDevice.get(),"proceduralSkyBox.frag");
        vk::PipelineVertexInputStateCreateInfo emptyVertexInputState{};
        emptyVertexInputState.setVertexBindingDescriptionCount(0);
        emptyVertexInputState.setVertexAttributeDescriptionCount(0);
        VulkanGraphicsPipeline pipeline(frame->backendDevice->device,vs->getStageCreateInfo(),fs->getStageCreateInfo(),emptyVertexInputState,renderPass,
                                        nullptr);
        pipeline.build();
        this->graphicsPipelines.push_back(pipeline);
    }

    void record(vk::CommandBuffer cmdBuf, int frameIdx) override
    {
        return;
        beginPass(cmdBuf);
        cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics,graphicsPipelines[0]._pipeline);
        cmdBuf.draw(3,1,0,0);
        endPass(cmdBuf);
    }
RASTERIZEDPASS_DEF_END(SkyBoxPass)

RASTERIZEDPASS_DEF_BEGIN(ShadowPass)
    void prepareAOT(const GPUFrame* frame) override
    {

    }

    void record(vk::CommandBuffer cmdBuf, int frameIdx) override
    {

    }
RASTERIZEDPASS_DEF_END(ShadowPass)

RASTERIZEDPASS_DEF_BEGIN(GBufferPass)
    void prepareAOT(const GPUFrame* frame) override
    {

    }

    void record(vk::CommandBuffer cmdBuf,int frameIdx) override
    {
        return;
        beginPass(cmdBuf);
        bindPassData(cmdBuf,frameIdx);
        for(auto & instanceRigidDynamic : scene->_dynamicRigidMeshBatch)
        {
            // how to define current pipeline?
            // shaders(variant),
            // vertex input, pipelineLayout could be retrieved through shaders reflection
            // renderPass is determined by pass itself
            // We ‘build and cache' pipeline, according to its identifier
            // renderContext = vertex input state + pipeline layout + shader + pipeline
            auto vertexInputState = instanceRigidDynamic.getVertexInputState();
            ShaderManager::ShaderMacroList macroList;
            if(instanceRigidDynamic.mesh->vertexAttribute.normalOffset != -1)
            {
                macroList.emplace_back("HAS_VERTEX_NORMAL","1");
            }
            if(instanceRigidDynamic.mesh->vertexAttribute.tangentOffset != -1 && instanceRigidDynamic.mesh->vertexAttribute.biTangentOffset != -1)
            {
                macroList.emplace_back("HAS_VERTEX_TANGENT_AND_BITANGENT","1");
            }
            if(instanceRigidDynamic.mesh->vertexAttribute.uvOffset != -1)
            {
                macroList.emplace_back("HAS_VERTEX_UV","1");
            }

            //VertexShader* vs = ShaderManager::getInstance().createVertexShader(backend_device,"simple.vert");
            //FragmentShader* fs = ShaderManager::getInstance().createFragmentShader(backend_device,"simple.frag");
            //Pass is responsible to check if the pipeline layout is compatible

            VulkanGraphicsPipeline *currentPipeline = nullptr; //= getGraphicsPipeline(vs,fs);
            cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics,currentPipeline->_pipeline);
            //mesh instance know how to bind the geometry buffer, how to draw
            instanceRigidDynamic.drawAll(cmdBuf);
        }
        endPass(cmdBuf);
    }

    renderScene::RenderScene* scene{};
    std::vector<std::pair<int,int>> pipelinesMap;
RASTERIZEDPASS_DEF_END(GBufferPass)

RASTERIZEDPASS_DEF_BEGIN(DeferredLightingPass)
    void prepareAOT(const GPUFrame* frame) override
    {

    }

    void record(vk::CommandBuffer cmdBuf,int frameIdx) override
    {

    }
RASTERIZEDPASS_DEF_END(DefereredLightingPass)

RASTERIZEDPASS_DEF_BEGIN(PostProcessPass)

    void prepareAOT(const GPUFrame* frame) override
    {
        auto vs = ShaderManager::getInstance().createVertexShader(frame->backendDevice.get(),"fullScreenQuad.vert");
        auto fs = ShaderManager::getInstance().createFragmentShader(frame->backendDevice.get(),"postProcess.frag");
        vk::PipelineVertexInputStateCreateInfo emptyVertexInputState{};
        emptyVertexInputState.setVertexBindingDescriptionCount(0);
        emptyVertexInputState.setVertexAttributeDescriptionCount(0);
        VulkanGraphicsPipeline pipeline(frame->backendDevice->device,vs->getStageCreateInfo(),fs->getStageCreateInfo(),emptyVertexInputState,renderPass,
                                        nullptr);
        pipeline.build();
        this->graphicsPipelines.push_back(pipeline);
    }

    void record(vk::CommandBuffer cmdBuf,int frameIdx) override
    {
        beginPass(cmdBuf);
        cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics,graphicsPipelines[0]._pipeline);
        cmdBuf.draw(3,1,0,0);
        endPass(cmdBuf);
    }

RASTERIZEDPASS_DEF_END(PostProcessPass)

#endif //PBRTEDITOR_PASSDEFINITION_H

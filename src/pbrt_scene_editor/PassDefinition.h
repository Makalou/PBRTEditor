//
// Created by 王泽远 on 2024/1/21.
//

#ifndef PBRTEDITOR_PASSDEFINITION_H
#define PBRTEDITOR_PASSDEFINITION_H

#include "GPUPass.h"

#define RASTERIZEDPASS_DEF_BEGIN(name) struct name : GPURasterizedPass { name() : GPURasterizedPass(#name){};

#define RASTERIZEDPASS_DEF_END(name) };

RASTERIZEDPASS_DEF_BEGIN(SkyBoxPass)
    void compileAOT() override
    {

    }

    void record(vk::CommandBuffer cmdBuf, int frameIdx) override
    {

    }
RASTERIZEDPASS_DEF_END(SkyBoxPass)

struct ShadowPass : GPURasterizedPass
{
    ShadowPass() : GPURasterizedPass("ShadowPass"){};
    void compileAOT() override
    {

    }

    void record(vk::CommandBuffer cmdBuf, int frameIdx) override
    {

    }
};

struct GBufferPass : GPURasterizedPass
{

    GBufferPass() : GPURasterizedPass("GBufferPass"){};
    void compileAOT() override
    {

    }

    void record(vk::CommandBuffer cmdBuf,int frameIdx) override
    {
        beginPass(cmdBuf);
        bindPassData(cmdBuf,frameIdx);
        endPass(cmdBuf);
        return;
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

            VertexShader* vs = ShaderManager::getInstance().createVertexShader(backend_device,"simple.vert");
            FragmentShader* fs = ShaderManager::getInstance().createFragmentShader(backend_device,"simple.frag");
            //Pass is responsible to check if the pipeline layout is compatible

            VulkanGraphicsPipeline *currentPipeline = nullptr; //= getGraphicsPipeline(vs,fs);
            cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics,currentPipeline->_pipeline);
            //mesh instance know how to bind the geometry buffer, how to draw
            instanceRigidDynamic.drawAll(cmdBuf);
        }
        endPass(cmdBuf);
    }

    renderScene::RenderScene* scene{};
};

struct DeferredLightingPass : GPURasterizedPass
{
    DeferredLightingPass() : GPURasterizedPass("DeferredLightingPass"){};

    void compileAOT() override
    {

    }

    void record(vk::CommandBuffer cmdBuf,int frameIdx) override
    {

    }

};

RASTERIZEDPASS_DEF_BEGIN(PostProcessPass)

    void compileAOT() override
    {

    }

    void record(vk::CommandBuffer cmdBuf,int frameIdx) override
    {

    }

RASTERIZEDPASS_DEF_END(PostProcessPass)

#endif //PBRTEDITOR_PASSDEFINITION_H

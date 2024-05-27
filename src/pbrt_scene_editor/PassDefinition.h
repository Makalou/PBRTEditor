//
// Created by 王泽远 on 2024/1/21.
//

#ifndef PBRTEDITOR_PASSDEFINITION_H
#define PBRTEDITOR_PASSDEFINITION_H

#include "GPUPass.h"
#include <map>

#define RASTERIZEDPASS_DEF_BEGIN(name) struct name : GPURasterizedPass { name() : GPURasterizedPass(#name){};

#define RASTERIZEDPASS_DEF_END(name) };

#define COMPUTEPASS_DEF_BEGIN(name) struct name : GPUComputePass { name() : GPUComputePass(#name){};

#define COMPUTEPASS_DEF_END(name) };

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
    void prepareAOT(GPUFrame* frame) override;
    renderScene::RenderScene* scene{};
RASTERIZEDPASS_DEF_END(SkyBoxPass)

RASTERIZEDPASS_DEF_BEGIN(ShadowPass)
    void prepareAOT(GPUFrame* frame) override;
RASTERIZEDPASS_DEF_END(ShadowPass)

RASTERIZEDPASS_DEF_BEGIN(SSAOPass)
    void prepareAOT(GPUFrame* frame) override;
    renderScene::RenderScene* scene{};
RASTERIZEDPASS_DEF_END(SSAOPass)

RASTERIZEDPASS_DEF_BEGIN(GBufferPass)
    vk::PipelineLayout passLevelPipelineLayout;
    vk::PipelineRasterizationStateCreateInfo rasterInfo{};
    vk::PipelineDepthStencilStateCreateInfo depthStencilInfo{};
    vk::PipelineColorBlendAttachmentState attachmentStates[8];
    vk::PipelineColorBlendStateCreateInfo colorBlendInfo{};
    vk::DescriptorSetLayout passDataDescriptorLayout;

    void prepareAOT(GPUFrame* frame) override;
    void prepareIncremental(GPUFrame* frame) override;

    using InstanceUUIDMap = std::unordered_map<renderScene::InstanceUUID,int,renderScene::InstanceUUIDHash>;
    InstanceUUIDMap pipelineLayoutMap;
    InstanceUUIDMap pipelineMap;
    InstanceUUIDMap instanceDescriptorSetMap;
    int currentInstanceDescriptorSetIdx = -1;
    std::vector<vk::PipelineLayout> instancePipelineLayouts;
    std::vector<vk::DescriptorSet> instanceDescriptorSets;

    vk::DescriptorSetLayout getPerInstanceDescriptorSetLayout()
    {
        return {};
    }

    int getOrCreatePipelineLayout(const GPUFrame* frame,const renderScene::InstanceBatchRigidDynamicType & instanceRigidDynamic)
    {
        if(instancePipelineLayouts.empty())
        {
            vk::PushConstantRange pushConstant{};
            pushConstant.setOffset(0);
            pushConstant.setSize(sizeof(glm::uvec4));
            pushConstant.setStageFlags(vk::ShaderStageFlagBits::eFragment);

            auto pipelineLayout = frame->backendDevice->createPipelineLayout2({frame->_frameGlobalDescriptorSetLayout,
                                                         passDataDescriptorLayout,
                                                         instanceRigidDynamic.getSetLayout()},{pushConstant});
            instancePipelineLayouts.push_back(pipelineLayout);
        }
        return 0;
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
        bool samePipelineLayout = true; //todo
        for(int i = 0; i< graphicsPipelines.size();i++)
        {
            if(graphicsPipelines[i].compatibleWith(vertexInputState) &&
               graphicsPipelines[i].compatibleWithVertexShader(vsUUID) &&
               graphicsPipelines[i].compatibleWithFragmentShader(fsUUID) &&
               samePipelineLayout)
            {
                pipelineMap.emplace(instanceRigidDynamic.getUUID(),i);
                idx = i;
                break;
            }
        }
        if(idx == -1)
        {
            // Need to create new pipeline
            auto* vs = ShaderManager::getInstance().createVertexShader(frame->backendDevice.get(),"simple.vert",macroList);
            auto* fs = ShaderManager::getInstance().createFragmentShader(frame->backendDevice.get(),"simple.frag",macroList);
            int pipelineLayoutIdx = getOrCreatePipelineLayout(frame,instanceRigidDynamic);
            VulkanGraphicsPipelineBuilder builder(frame->backendDevice->device,vs,fs,
                vertexInputState.getCreateInfo(),
                renderPass,
                instancePipelineLayouts[pipelineLayoutIdx],
                rasterInfo,depthStencilInfo,colorBlendInfo);
            auto pipeline = builder.build();
            graphicsPipelines.push_back(pipeline);
            idx = graphicsPipelines.size() - 1;
        }
        return idx;
    }

    int allocateDescriptorSet(const GPUFrame* frame, const renderScene::InstanceBatchRigidDynamicType& instanceRigidDynamic)
    {
        return 0;
    }

    void bindRenderState(PassActionContext& actionCtx, const GPUFrame* frame, const renderScene::InstanceBatchRigidDynamicType & instanceRigidDynamic)
    {
        auto instUUID = instanceRigidDynamic.getUUID();
        auto pipelineIdx = pipelineMap.find(instUUID);
        if(pipelineIdx!=pipelineMap.end())
        {
            actionCtx.pipelineIdx = pipelineIdx->second;
        }else{
            actionCtx.pipelineIdx = getOrCreatePipeline(frame,instanceRigidDynamic);
        }
        /*auto descriptorSetIdx = instanceDescriptorSetMap.find(instUUID);
        if (descriptorSetIdx != instanceDescriptorSetMap.end())
        {
            bindCurrentInstanceDescriptorSet(cmdBuf, descriptorSetIdx->second);
        }
        else {
            auto idx = allocateDescriptorSet(frame, instanceRigidDynamic);
            bindCurrentInstanceDescriptorSet(cmdBuf, idx);
        }*/
        actionCtx.descriptorSets.push_back(instanceRigidDynamic.getDescriptorSet());
    }

    renderScene::RenderScene* scene{};
    std::vector<std::pair<int,int>> pipelinesMap;
RASTERIZEDPASS_DEF_END(GBufferPass)

RASTERIZEDPASS_DEF_BEGIN(DeferredLightingPass)
    void prepareAOT(GPUFrame* frame) override;
RASTERIZEDPASS_DEF_END(DefereredLightingPass)

RASTERIZEDPASS_DEF_BEGIN(PostProcessPass)
    void prepareAOT(GPUFrame* frame) override;

RASTERIZEDPASS_DEF_END(PostProcessPass)

RASTERIZEDPASS_DEF_BEGIN(CopyPass)
    glm::uvec4 currentTexIdx{};

    void prepareAOT(GPUFrame* frame) override;
RASTERIZEDPASS_DEF_END(CopyPass)

RASTERIZEDPASS_DEF_BEGIN(SelectedMaskPass)
    renderScene::RenderScene* scene{};
    vk::PipelineLayout pipelineLayout = VK_NULL_HANDLE;
    vk::DescriptorSetLayout passDataDescriptorLayout;

    void prepareAOT(GPUFrame* frame) override;
    void prepareIncremental(GPUFrame* frame) override;
RASTERIZEDPASS_DEF_END(SelectedMaskPass)

RASTERIZEDPASS_DEF_BEGIN(BoundingBoxPass)

RASTERIZEDPASS_DEF_END(BoundingBoxPass)

RASTERIZEDPASS_DEF_BEGIN(WireFramePass)
    renderScene::RenderScene* scene{};
    vk::PipelineLayout pipelineLayout = VK_NULL_HANDLE;
    vk::DescriptorSetLayout passDataDescriptorLayout;
    void prepareAOT(GPUFrame* frame) override;
    void prepareIncremental(GPUFrame* frame) override;
RASTERIZEDPASS_DEF_END(WireFramePass)

RASTERIZEDPASS_DEF_BEGIN(OutlinePass)
    void prepareAOT(GPUFrame* frame) override;
RASTERIZEDPASS_DEF_END(OutlinePass)

COMPUTEPASS_DEF_BEGIN(ObjectPickPass)
    vk::PipelineLayout pipelineLayout;
    vk::Pipeline pipeline;
    vk::DescriptorSetLayout passDataDescriptorLayout;
    VMAObservedBufferMapped<glm::uvec4> objectIDBuffer;
    renderScene::RenderScene* scene{};

    void prepareAOT(GPUFrame* frame) override;
COMPUTEPASS_DEF_END(ObjectPickPass)
#endif //PBRTEDITOR_PASSDEFINITION_H

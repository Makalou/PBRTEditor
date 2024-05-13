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
    vk::DescriptorSetLayout passDataDescriptorLayout;
    vk::PipelineLayout pipelineLayout;
    void prepareAOT(GPUFrame* frame) override
    {
        auto vs = FullScreenQuadDrawer::getVertexShader(frame->backendDevice.get());
        auto fs = ShaderManager::getInstance().createFragmentShader(frame->backendDevice.get(),"proceduralSkyBox.frag");

        std::vector<vk::DescriptorSetLayoutBinding> bindings;
        vk::DescriptorSetLayoutBinding camBinding;
        camBinding.setBinding(0);
        camBinding.setDescriptorType(vk::DescriptorType::eUniformBuffer);
        camBinding.setDescriptorCount(1);
        camBinding.setStageFlags(vk::ShaderStageFlagBits::eAllGraphics);
        bindings.push_back(camBinding);

        passDataDescriptorLayout = frame->manageDescriptorSet("SkyBoxPassDataDescriptorSet",bindings);

        frame->getManagedDescriptorSet("SkyBoxPassDataDescriptorSet",[frame,this](const vk::DescriptorSet & passDataDescriptorSet) mutable {
            frame->backendDevice->updateDescriptorSetUniformBuffer(passDataDescriptorSet,0,scene->mainView.camera.data.getBuffer());
        });

        pipelineLayout = frame->backendDevice->createPipelineLayout2({frame->_frameGlobalDescriptorSetLayout,passDataDescriptorLayout});
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
        cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,pipelineLayout,1,frame->getManagedDescriptorSet("SkyBoxPassDataDescriptorSet"),
                                  nullptr);
        cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics,graphicsPipelines[0].getPipeline());
        FullScreenQuadDrawer::draw(cmdBuf);
        endPass(cmdBuf);
    }

    renderScene::RenderScene* scene{};
RASTERIZEDPASS_DEF_END(SkyBoxPass)

RASTERIZEDPASS_DEF_BEGIN(ShadowPass)
    void prepareAOT(GPUFrame* frame) override
    {

    }

    void record(vk::CommandBuffer cmdBuf, const GPUFrame* frame) override
    {

    }
RASTERIZEDPASS_DEF_END(ShadowPass)

RASTERIZEDPASS_DEF_BEGIN(GBufferPass)
    vk::PipelineLayout passLevelPipelineLayout;
    vk::PipelineRasterizationStateCreateInfo rasterInfo{};
    vk::PipelineDepthStencilStateCreateInfo depthStencilInfo{};
    vk::PipelineColorBlendAttachmentState attachmentStates[8];
    vk::PipelineColorBlendStateCreateInfo colorBlendInfo{};
    vk::DescriptorSetLayout passDataDescriptorLayout;

    void prepareAOT(GPUFrame* frame) override
    {
        std::vector<vk::DescriptorSetLayoutBinding> bindings;
        vk::DescriptorSetLayoutBinding camBinding;
        camBinding.setBinding(0);
        camBinding.setDescriptorType(vk::DescriptorType::eUniformBuffer);
        camBinding.setDescriptorCount(1);
        camBinding.setStageFlags(vk::ShaderStageFlagBits::eAllGraphics);
        bindings.push_back(camBinding);

        passDataDescriptorLayout = frame->manageDescriptorSet("GBufferPassDataDescriptorSet",bindings);

        frame->getManagedDescriptorSet("GBufferPassDataDescriptorSet",[frame,this](const vk::DescriptorSet & passDataDescriptorSet) mutable {
            frame->backendDevice->updateDescriptorSetUniformBuffer(passDataDescriptorSet,0,scene->mainView.camera.data.getBuffer());
        });

        passLevelPipelineLayout = frame->backendDevice->createPipelineLayout2({frame->_frameGlobalDescriptorSetLayout,passDataDescriptorLayout});
        frame->backendDevice->setObjectDebugName(passLevelPipelineLayout, "GBufferPassLevelPipelineLayout");

        rasterInfo.setCullMode(vk::CullModeFlagBits::eNone);
        rasterInfo.setRasterizerDiscardEnable(vk::False);
        rasterInfo.setFrontFace(vk::FrontFace::eClockwise);
        rasterInfo.setPolygonMode(vk::PolygonMode::eFill);
        rasterInfo.setLineWidth(1.0);

        depthStencilInfo.setDepthTestEnable(vk::True);
        depthStencilInfo.setDepthWriteEnable(vk::True);
        depthStencilInfo.setDepthCompareOp(vk::CompareOp::eLess);
        depthStencilInfo.setDepthBoundsTestEnable(vk::False);
        depthStencilInfo.setMinDepthBounds(0.0f);
        depthStencilInfo.setMaxDepthBounds(1.0f);
        depthStencilInfo.setStencilTestEnable(vk::False);

        auto colorComponentAll = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
        for (int i = 0; i < 8; i++)
        {
            attachmentStates[i].setColorWriteMask(colorComponentAll);
            attachmentStates[i].setSrcColorBlendFactor(vk::BlendFactor::eOne);
            attachmentStates[i].setDstColorBlendFactor(vk::BlendFactor::eZero);
            attachmentStates[i].setColorBlendOp(vk::BlendOp::eAdd);
            attachmentStates[i].setSrcAlphaBlendFactor(vk::BlendFactor::eOne);
            attachmentStates[i].setDstAlphaBlendFactor(vk::BlendFactor::eZero);
            attachmentStates[i].setAlphaBlendOp(vk::BlendOp::eAdd);
            attachmentStates[i].blendEnable = VK_FALSE;
        }

        colorBlendInfo.setLogicOpEnable(vk::False);
        colorBlendInfo.setAttachments(attachmentStates);
        colorBlendInfo.setBlendConstants({ 1.0,1.0,1.0,1.0 });
    }

    using InstanceUUIDMap = std::unordered_map<renderScene::InstanceUUID,int,renderScene::InstanceUUIDHash>;
    InstanceUUIDMap pipelineLayoutMap;
    InstanceUUIDMap pipelineMap;
    InstanceUUIDMap instanceDescriptorSetMap;
    int currentPipelineIdx = -1;
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
            cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,graphicsPipelines[currentPipelineIdx].getLayout(),3,
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
        /*auto descriptorSetIdx = instanceDescriptorSetMap.find(instUUID);
        if (descriptorSetIdx != instanceDescriptorSetMap.end())
        {
            bindCurrentInstanceDescriptorSet(cmdBuf, descriptorSetIdx->second);
        }
        else {
            auto idx = allocateDescriptorSet(frame, instanceRigidDynamic);
            bindCurrentInstanceDescriptorSet(cmdBuf, idx);
        }*/
        cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,graphicsPipelines[currentPipelineIdx].getPipelineLayout(),
                                  2,
                                  instanceRigidDynamic.getDescriptorSet(),
                                  nullptr);
    }

    void record(vk::CommandBuffer cmdBuf,const GPUFrame* frame) override
    {
        cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,passLevelPipelineLayout,1,frame->getManagedDescriptorSet("GBufferPassDataDescriptorSet"),
                                  nullptr);
        beginPass(cmdBuf);

        currentPipelineIdx = -1;
        currentInstanceDescriptorSetIdx = -1;

        if(scene!= nullptr && !scene->_dynamicRigidMeshBatch.empty())
        {
            auto view = scene->mainView.camera.data->view;
            auto proj = scene->mainView.camera.data->proj;
            auto vp = proj * view;

            int visibleCount = 0;

            for(int i = 0; i < scene->_dynamicRigidMeshBatch.size(); i++)
            {
                auto & instanceRigidDynamic = scene->_dynamicRigidMeshBatch[i];
                // renderState = vertex input state + pipeline layout + shader + pipeline
                // todo : Here is a big problem. The performance is unpredictable.
                bindRenderState(cmdBuf,frame,instanceRigidDynamic);
                glm::uvec4 meshIdx;
                meshIdx.x = i;
                meshIdx.y = scene->_dynamicRigidMeshBatch.size();
                cmdBuf.pushConstants(graphicsPipelines[currentPipelineIdx].getPipelineLayout(), vk::ShaderStageFlagBits::eFragment,0, sizeof(glm::uvec4), &meshIdx);
                //mesh instance know how to bind the geometry buffer, how to draw
                instanceRigidDynamic.drawAll(cmdBuf);
                //instanceRigidDynamic.drawOne(cmdBuf);
               /* visibleCount += instanceRigidDynamic.drawCulled(cmdBuf,[&](auto meshHandle, const auto & perInstanceData) -> bool {
                    auto aabb = scene->aabbs[meshHandle.idx];
                    glm::vec3 corners[8]{};
                    corners[0] = {aabb.minX, -aabb.minY, aabb.minZ};
                    corners[1] = {aabb.maxX, -aabb.minY, aabb.minZ};
                    corners[2] = {aabb.minX, -aabb.maxY, aabb.minZ};
                    corners[3] = {aabb.minX, -aabb.minY, aabb.maxZ};
                    corners[4] = {aabb.maxX, -aabb.maxY, aabb.minZ};
                    corners[5] = {aabb.maxX, -aabb.minY, aabb.maxZ};
                    corners[6] = {aabb.minX, -aabb.maxY, aabb.maxZ};
                    corners[7] = {aabb.maxX, -aabb.maxY, aabb.maxZ};

                    for(int i = 0; i < 8; i ++)
                    {
                        auto clip_space_corner = vp * glm::vec4(corners[i],1.0);
                        auto x = clip_space_corner.x;
                        auto y = clip_space_corner.y;
                        auto z = clip_space_corner.z;
                        auto w = clip_space_corner.w;

                        if((x >= -w) && (x <= w) && (y>=-w) && (y<=w) && (z >= 0) && (z <= w))
                            return true;
                    }

                    return false;
                });*/
            }
        }
        endPass(cmdBuf);
    }

    renderScene::RenderScene* scene{};
    std::vector<std::pair<int,int>> pipelinesMap;
RASTERIZEDPASS_DEF_END(GBufferPass)

RASTERIZEDPASS_DEF_BEGIN(DeferredLightingPass)
    vk::PipelineLayout pipelineLayout;
    void prepareAOT(GPUFrame* frame) override
    {
        auto vs = FullScreenQuadDrawer::getVertexShader(frame->backendDevice.get());
        auto fs = ShaderManager::getInstance().createFragmentShader(frame->backendDevice.get(),"deferred.frag");

        pipelineLayout = frame->backendDevice->createPipelineLayout2({frame->_frameGlobalDescriptorSetLayout,passInputDescriptorSetLayout});
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
        auto passInputDescriptorSet = frame->getManagedDescriptorSet("DeferredLightingPassInputDescriptorSet");
        cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 1, passInputDescriptorSet, nullptr);
        cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics,graphicsPipelines[0].getPipeline());
        FullScreenQuadDrawer::draw(cmdBuf);
        endPass(cmdBuf);
    }
RASTERIZEDPASS_DEF_END(DefereredLightingPass)

RASTERIZEDPASS_DEF_BEGIN(PostProcessPass)
    vk::PipelineLayout pipelineLayout;
    void prepareAOT(GPUFrame* frame) override
    {
        auto vs = FullScreenQuadDrawer::getVertexShader(frame->backendDevice.get());
        auto fs = ShaderManager::getInstance().createFragmentShader(frame->backendDevice.get(),"postProcess.frag");

        pipelineLayout = frame->backendDevice->createPipelineLayout2({frame->_frameGlobalDescriptorSetLayout,passInputDescriptorSetLayout});
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
        auto passInputDescriptorSet = frame->getManagedDescriptorSet("PostProcessPassInputDescriptorSet");
        cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,pipelineLayout,1,passInputDescriptorSet, nullptr);
        cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics,graphicsPipelines[0].getPipeline());
        FullScreenQuadDrawer::draw(cmdBuf);
        endPass(cmdBuf);
    }

RASTERIZEDPASS_DEF_END(PostProcessPass)

RASTERIZEDPASS_DEF_BEGIN(CopyPass)
    vk::PipelineLayout pipelineLayout;
    glm::uvec4 currentTexIdx{};

    void prepareAOT(GPUFrame* frame) override
    {
        auto vs = FullScreenQuadDrawer::getVertexShader(frame->backendDevice.get());
        auto fs = ShaderManager::getInstance().createFragmentShader(frame->backendDevice.get(), "copy.frag");

        vk::PushConstantRange pushConstant{};
        pushConstant.setOffset(0);
        pushConstant.setSize(sizeof(glm::uvec4));
        pushConstant.setStageFlags(vk::ShaderStageFlagBits::eFragment);

        pipelineLayout = frame->backendDevice->createPipelineLayout2({ frame->_frameGlobalDescriptorSetLayout,
                                                                                passInputDescriptorSetLayout }, {pushConstant});

        frame->backendDevice->setObjectDebugName(pipelineLayout, "CopyPassPipelineLayout");

        VulkanGraphicsPipelineBuilder builder(frame->backendDevice->device, vs, fs,
            FullScreenQuadDrawer::getVertexInputStateInfo(), renderPass,
            pipelineLayout);
        auto pipeline = builder.build();
        frame->backendDevice->setObjectDebugName(pipeline.getPipeline(), "CopyPassPipeline");
        graphicsPipelines.push_back(pipeline);
    }

    void record(vk::CommandBuffer cmdBuf, const GPUFrame* frame) override
    {
        beginPass(cmdBuf);
        auto passInputDescriptorSet = frame->getManagedDescriptorSet("CopyPassInputDescriptorSet");
        cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipelines[0].getPipeline());
        cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 1, passInputDescriptorSet, nullptr);
        cmdBuf.pushConstants(pipelineLayout, vk::ShaderStageFlagBits::eFragment,0, sizeof(glm::uvec4), &currentTexIdx);
        FullScreenQuadDrawer::draw(cmdBuf);
        endPass(cmdBuf);
    }
RASTERIZEDPASS_DEF_END(CopyPass)

RASTERIZEDPASS_DEF_BEGIN(SelectedMaskPass)
    renderScene::RenderScene* scene{};
    vk::PipelineLayout pipelineLayout = VK_NULL_HANDLE;
    vk::DescriptorSetLayout passDataDescriptorLayout;
    void prepareAOT(GPUFrame* frame) override
    {
        std::vector<vk::DescriptorSetLayoutBinding> bindings;
        vk::DescriptorSetLayoutBinding camBinding;
        camBinding.setBinding(0);
        camBinding.setDescriptorType(vk::DescriptorType::eUniformBuffer);
        camBinding.setDescriptorCount(1);
        camBinding.setStageFlags(vk::ShaderStageFlagBits::eAllGraphics);
        bindings.push_back(camBinding);

        passDataDescriptorLayout = frame->manageDescriptorSet("SelectedMaskPassDataDescriptorSet", bindings);

        frame->getManagedDescriptorSet("SelectedMaskPassDataDescriptorSet", [frame, this](const vk::DescriptorSet& passDataDescriptorSet) mutable {
            frame->backendDevice->updateDescriptorSetUniformBuffer(passDataDescriptorSet, 0, scene->mainView.camera.data.getBuffer());
        });
    }

    void record(vk::CommandBuffer cmdBuf, const GPUFrame* frame) override
    {
        beginPass(cmdBuf);
        auto passDataDescriptorSet = frame->getManagedDescriptorSet("SelectedMaskPassDataDescriptorSet");
        for (int i = 0; i < scene->_dynamicRigidMeshBatch.size(); i++)
        {
            auto& instanceRigidDynamic = scene->_dynamicRigidMeshBatch[i];
            if (graphicsPipelines.empty())
            {
                pipelineLayout = frame->backendDevice->createPipelineLayout2({ frame->_frameGlobalDescriptorSetLayout,passDataDescriptorLayout,instanceRigidDynamic.getSetLayout()});
                frame->backendDevice->setObjectDebugName(pipelineLayout, "SelectedMaskPassPipelineLayout");
                auto vs = ShaderManager::getInstance().createVertexShader(frame->backendDevice.get(), "positionOnly.vert");
                auto fs = ShaderManager::getInstance().createFragmentShader(frame->backendDevice.get(), "constantColor.frag");
                VulkanGraphicsPipelineBuilder builder(frame->backendDevice->device, vs, fs,
                    instanceRigidDynamic.getPosOnlyVertexInputState().getCreateInfo(), renderPass,
                    pipelineLayout);
                builder.addDynamicState(vk::DynamicState::eVertexInputBindingStride);
                auto pipeline = builder.build();
                frame->backendDevice->setObjectDebugName(pipeline.getPipeline(), "SelectedMaskPassPipeline");
                graphicsPipelines.push_back(pipeline);
            }
            if (instanceRigidDynamic.updateCurrentMask(1, frame->backendDevice.get()) > 0)
            {
                cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipelines[0].getPipeline());
                cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 1, passDataDescriptorSet, nullptr);
                cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 2, instanceRigidDynamic.getDescriptorSet(), nullptr);
                instanceRigidDynamic.drawAllPosOnlyMasked(cmdBuf,frame->backendDevice->getDLD());
            }
        }

        endPass(cmdBuf);
    }
RASTERIZEDPASS_DEF_END(SelectedMaskPass)

RASTERIZEDPASS_DEF_BEGIN(BoundingBoxPass)

RASTERIZEDPASS_DEF_END(BoundingBoxPass)

RASTERIZEDPASS_DEF_BEGIN(WireFramePass)
    renderScene::RenderScene* scene{};
    vk::PipelineLayout pipelineLayout = VK_NULL_HANDLE;
    vk::DescriptorSetLayout passDataDescriptorLayout;
    void prepareAOT(GPUFrame* frame) override
    {
        std::vector<vk::DescriptorSetLayoutBinding> bindings;
        vk::DescriptorSetLayoutBinding camBinding;
        camBinding.setBinding(0);
        camBinding.setDescriptorType(vk::DescriptorType::eUniformBuffer);
        camBinding.setDescriptorCount(1);
        camBinding.setStageFlags(vk::ShaderStageFlagBits::eAllGraphics);
        bindings.push_back(camBinding);

        passDataDescriptorLayout = frame->manageDescriptorSet("WireFramePassDataDescriptorSet", bindings);

        frame->getManagedDescriptorSet("WireFramePassDataDescriptorSet", [frame, this](const vk::DescriptorSet& passDataDescriptorSet) mutable {
            frame->backendDevice->updateDescriptorSetUniformBuffer(passDataDescriptorSet, 0, scene->mainView.camera.data.getBuffer());
        });
    }

    void record(vk::CommandBuffer cmdBuf, const GPUFrame* frame) override
    {
        beginPass(cmdBuf);
        auto passDataDescriptorSet = frame->getManagedDescriptorSet("WireFramePassDataDescriptorSet");
        for (int i = 0; i < scene->_dynamicRigidMeshBatch.size(); i++)
        {
            auto& instanceRigidDynamic = scene->_dynamicRigidMeshBatch[i];
            if (graphicsPipelines.empty())
            {
                pipelineLayout = frame->backendDevice->createPipelineLayout2({ frame->_frameGlobalDescriptorSetLayout,passDataDescriptorLayout,instanceRigidDynamic.getSetLayout() });
                frame->backendDevice->setObjectDebugName(pipelineLayout, "WireFramePassPipelineLayout");
                auto vs = ShaderManager::getInstance().createVertexShader(frame->backendDevice.get(), "positionOnly.vert");
                auto fs = ShaderManager::getInstance().createFragmentShader(frame->backendDevice.get(), "constantColor.frag");

                vk::PipelineRasterizationStateCreateInfo rasterInfo{};
                rasterInfo.setCullMode(vk::CullModeFlagBits::eNone);
                rasterInfo.setRasterizerDiscardEnable(vk::False);
                rasterInfo.setFrontFace(vk::FrontFace::eClockwise);
                rasterInfo.setPolygonMode(vk::PolygonMode::eLine);
                rasterInfo.setLineWidth(2.0);

                vk::PipelineDepthStencilStateCreateInfo depthStencilInfo{};
                depthStencilInfo.setDepthTestEnable(vk::True);
                depthStencilInfo.setDepthWriteEnable(vk::True);
                depthStencilInfo.setDepthCompareOp(vk::CompareOp::eLess);
                depthStencilInfo.setDepthBoundsTestEnable(vk::False);
                depthStencilInfo.setMinDepthBounds(0.0f);
                depthStencilInfo.setMaxDepthBounds(1.0f);
                depthStencilInfo.setStencilTestEnable(vk::False);

                VulkanGraphicsPipelineBuilder builder(frame->backendDevice->device, vs, fs,
                    instanceRigidDynamic.getPosOnlyVertexInputState().getCreateInfo(), renderPass,
                    pipelineLayout,rasterInfo,depthStencilInfo);
                builder.addDynamicState(vk::DynamicState::eVertexInputBindingStride);
                auto pipeline = builder.build();
                frame->backendDevice->setObjectDebugName(pipeline.getPipeline(), "WireFramePassPipeline");
                graphicsPipelines.push_back(pipeline);
            }
            cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipelines[0].getPipeline());
            cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 1, passDataDescriptorSet, nullptr);
            cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 2, instanceRigidDynamic.getDescriptorSet(), nullptr);
            instanceRigidDynamic.drawAllPosOnly(cmdBuf, frame->backendDevice->getDLD());
        }

        endPass(cmdBuf);
    }
RASTERIZEDPASS_DEF_END(WireFramePass)

RASTERIZEDPASS_DEF_BEGIN(OutlinePass)
    vk::PipelineLayout pipelineLayout;
    void prepareAOT(GPUFrame* frame) override
    {
        auto vs = FullScreenQuadDrawer::getVertexShader(frame->backendDevice.get());
        auto fs = ShaderManager::getInstance().createFragmentShader(frame->backendDevice.get(), "outline.frag");

        pipelineLayout = frame->backendDevice->createPipelineLayout2({ frame->_frameGlobalDescriptorSetLayout,passInputDescriptorSetLayout });
        frame->backendDevice->setObjectDebugName(pipelineLayout, "OutlinePassPipelineLayout");

        vk::PipelineColorBlendAttachmentState attachmentState{};
        attachmentState.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
        attachmentState.blendEnable = vk::True;
        attachmentState.srcColorBlendFactor = vk::BlendFactor::eOne;
        attachmentState.dstColorBlendFactor = vk::BlendFactor::eDstAlpha;
        attachmentState.colorBlendOp = vk::BlendOp::eAdd;
        attachmentState.srcAlphaBlendFactor = vk::BlendFactor::eOne;
        attachmentState.dstAlphaBlendFactor = vk::BlendFactor::eZero;
        attachmentState.alphaBlendOp = vk::BlendOp::eAdd;
        vk::PipelineColorBlendStateCreateInfo colorBlendInfo{};
        colorBlendInfo.setAttachments(attachmentState);
        colorBlendInfo.setLogicOpEnable(vk::False);

        VulkanGraphicsPipelineBuilder builder(frame->backendDevice->device, vs, fs,
            FullScreenQuadDrawer::getVertexInputStateInfo(), renderPass,
            pipelineLayout,colorBlendInfo);
        auto pipeline = builder.build();
        frame->backendDevice->setObjectDebugName(pipeline.getPipeline(), "OutlinePassPipeline");
        graphicsPipelines.push_back(pipeline);
    }

    void record(vk::CommandBuffer cmdBuf, const GPUFrame* frame) override
    {
        beginPass(cmdBuf);
        auto passInputDescriptorSet = frame->getManagedDescriptorSet("OutlinePassInputDescriptorSet");
        cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 1, passInputDescriptorSet, nullptr);
        cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipelines[0].getPipeline());
        FullScreenQuadDrawer::draw(cmdBuf);
        endPass(cmdBuf);
    }
RASTERIZEDPASS_DEF_END(OutlinePass)

COMPUTEPASS_DEF_BEGIN(ObjectPickPass)
    vk::PipelineLayout pipelineLayout;
    vk::Pipeline pipeline;
    vk::DescriptorSetLayout passDataDescriptorLayout;
    VMAObservedBufferMapped<glm::uvec4> objectIDBuffer;
    renderScene::RenderScene* scene{};

    void prepareAOT(GPUFrame* frame) override
    {
        auto resBuffer = frame->backendDevice->allocateObservedBufferPull<glm::uvec4>(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        if (!resBuffer)
        {
            throw std::runtime_error("Failed to create objectIDBuffer");
        }
        objectIDBuffer = resBuffer.value();
        vk::DescriptorSetLayoutBinding binding;
        binding.setBinding(0);
        binding.setDescriptorType(vk::DescriptorType::eStorageBuffer);
        binding.setDescriptorCount(1);
        binding.setStageFlags(vk::ShaderStageFlagBits::eCompute);
        passDataDescriptorLayout = frame->manageDescriptorSet("ObjectPickPassDataDescriptorSet", {binding});
        frame->getManagedDescriptorSet("ObjectPickPassDataDescriptorSet", [frame, this](const vk::DescriptorSet& passDataDescriptorSet) mutable {
            frame->backendDevice->updateDescriptorSetStorageBuffer(passDataDescriptorSet, 0, objectIDBuffer.getBuffer());
        });

        pipelineLayout = frame->backendDevice->createPipelineLayout2({ frame->_frameGlobalDescriptorSetLayout,
            passInputDescriptorSetLayout, passDataDescriptorLayout });

        auto cs = ShaderManager::getInstance().createComputeShader(frame->backendDevice.get(), "objectPick.comp");
        vk::ComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.setLayout(pipelineLayout);
        pipelineInfo.setStage(cs->getStageCreateInfo());

        auto newPipeline = frame->backendDevice->createComputePipeline(VK_NULL_HANDLE, pipelineInfo);
        if (newPipeline.result != vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to create ObjectPickPassPipeline");
        }
        pipeline = newPipeline.value;
        frame->backendDevice->setObjectDebugName(pipelineLayout, "ObjectPickPassPipelineLayout");
        frame->backendDevice->setObjectDebugName(pipeline, "ObjectPickPassPipeline");
    }

    void record(vk::CommandBuffer cmdBuf, const GPUFrame* frame) override
    {
        auto passInputDescriptorSet = frame->getManagedDescriptorSet("ObjectPickPassInputDescriptorSet");
        cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipelineLayout, 1, passInputDescriptorSet, nullptr);
        auto passDataDescriptorSet = frame->getManagedDescriptorSet("ObjectPickPassDataDescriptorSet");
        cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipelineLayout, 2, passDataDescriptorSet, nullptr);
        cmdBuf.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline);
        cmdBuf.dispatch(1, 1, 1);
        scene->selectedDynamicRigidMeshID.x = objectIDBuffer->x;
        scene->selectedDynamicRigidMeshID.y = objectIDBuffer->y;
    }
COMPUTEPASS_DEF_END(ObjectPickPass)
#endif //PBRTEDITOR_PASSDEFINITION_H

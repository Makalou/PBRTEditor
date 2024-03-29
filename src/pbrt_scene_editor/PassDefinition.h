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

    void prepareAOT(GPUFrame* frame) override
    {
        std::vector<vk::DescriptorSetLayoutBinding> bindings;
        vk::DescriptorSetLayoutBinding camBinding;
        camBinding.setBinding(0);
        camBinding.setDescriptorType(vk::DescriptorType::eUniformBuffer);
        camBinding.setDescriptorCount(1);
        camBinding.setStageFlags(vk::ShaderStageFlagBits::eAllGraphics);
        bindings.push_back(camBinding);

        auto passDataDescriptorLayout = frame->manageDescriptorSet("GBufferPassDataDescriptorSet",bindings);

        frame->getManagedDescriptorSet("GBufferPassDataDescriptorSet",[frame,this](const vk::DescriptorSet & passDataDescriptorSet) mutable {
            frame->backendDevice->updateDescriptorSetUniformBuffer(passDataDescriptorSet,0,scene->mainView.camera.data.getBuffer());
        });

        passLevelPipelineLayout = frame->backendDevice->createPipelineLayout2({frame->_frameGlobalDescriptorSetLayout,passDataDescriptorLayout});

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
            VulkanGraphicsPipelineBuilder builder(frame->backendDevice->device,vs,fs,
                vertexInputState.getCreateInfo(),
                renderPass,
                passLevelPipelineLayout,
                rasterInfo,depthStencilInfo);
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
    }

    void record(vk::CommandBuffer cmdBuf,const GPUFrame* frame) override
    {
        cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,passLevelPipelineLayout,1,frame->getManagedDescriptorSet("GBufferPassDataDescriptorSet"),
                                  nullptr);
        beginPass(cmdBuf);

        if(scene!= nullptr)
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
                //mesh instance know how to bind the geometry buffer, how to draw
                //instanceRigidDynamic.drawAll(cmdBuf);
                visibleCount += instanceRigidDynamic.drawCulled(cmdBuf,[&](auto meshHandle, const auto & perInstanceData) -> bool {
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
                });
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

#endif //PBRTEDITOR_PASSDEFINITION_H

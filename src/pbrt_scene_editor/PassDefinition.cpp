#include "PassDefinition.h"
#include "editorGUI.h"

void SkyBoxPass::prepareAOT(FrameCoordinator* coordinator)
{
    auto vs = FullScreenQuadDrawer::getVertexShader(coordinator->backendDevice);
    auto fs = ShaderManager::getInstance().createFragmentShader(coordinator->backendDevice, "proceduralSkyBox.frag");

    auto passDataDescriptorLayout = coordinator->manageInFlightDescriptorSetAOT("SkyBoxPassDataDescriptorSet", { {vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eAllGraphics } });

    coordinator->updateInFlightDescriptorSetAOT("SkyBoxPassDataDescriptorSet", 0, vk::DescriptorType::eUniformBuffer, [this](GPUFrame* frame) {
        return vk::Buffer(scene->mainView.camera.data.getBufferFor(frame->frameIdx));
    });

    auto pipelineLayout = coordinator->backendDevice->createPipelineLayout2({ coordinator->getFrameGlobalDescriptorSetLayout(),passDataDescriptorLayout});
    coordinator->backendDevice->setObjectDebugName(pipelineLayout, "SkyBoxPassPipelineLayout");

    VulkanGraphicsPipelineBuilder builder(coordinator->backendDevice->device, vs, fs,
        FullScreenQuadDrawer::getVertexInputStateInfo(), renderPass,
        pipelineLayout);
    auto pipeline = builder.build();
    coordinator->backendDevice->setObjectDebugName(pipeline.getPipeline(), "SkyBoxPassPipeline");
    graphicsPipelines.push_back(pipeline);
    PassActionContext actionContext{};
    actionContext.pipelineIdx = 0;
    actionContext.firstSet = 1;
    actionContext.descriptorSets = { "SkyBoxPassDataDescriptorSet" };
    actionContext.action = [](vk::CommandBuffer cmd, uint32_t pipelineIdx) {FullScreenQuadDrawer::draw(cmd); };
    actionContextQueue.push_back(actionContext);
}

void ShadowPass::prepareAOT(FrameCoordinator* coordinator)
{

}

void SSAOPass::prepareAOT(FrameCoordinator* coordinator)
{
    auto vs = FullScreenQuadDrawer::getVertexShader(coordinator->backendDevice);
    auto fs = ShaderManager::getInstance().createFragmentShader(coordinator->backendDevice, "ssao.frag");

    auto passDataDescriptorLayout = coordinator->manageInFlightDescriptorSetAOT("SSAOPassDataDescriptorSet", { {vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eAllGraphics } });

    coordinator->updateInFlightDescriptorSetAOT("SSAOPassDataDescriptorSet", 0, vk::DescriptorType::eUniformBuffer, [this](GPUFrame* frame) {
        return vk::Buffer(scene->mainView.camera.data.getBufferFor(frame->frameIdx));
    });

    auto pipelineLayout = coordinator->backendDevice->createPipelineLayout2({ coordinator->getFrameGlobalDescriptorSetLayout(), passInputDescriptorSetLayout, passDataDescriptorLayout});
    coordinator->backendDevice->setObjectDebugName(pipelineLayout, "SSAOPassPipelineLayout");

    VulkanGraphicsPipelineBuilder builder(coordinator->backendDevice->device, vs, fs,
        FullScreenQuadDrawer::getVertexInputStateInfo(), renderPass,
        pipelineLayout);
    auto pipeline = builder.build();
    coordinator->backendDevice->setObjectDebugName(pipeline.getPipeline(), "SSAOPassPipeline");
    graphicsPipelines.push_back(pipeline);
    PassActionContext actionContext{};
    actionContext.pipelineIdx = 0;
    actionContext.firstSet = 1;
    actionContext.descriptorSets = { "SSAOPassInputDescriptorSet","SSAOPassDataDescriptorSet" };
    actionContext.action = [](vk::CommandBuffer cmd, uint32_t pipelineIdx) {FullScreenQuadDrawer::draw(cmd); };
    actionContextQueue.push_back(actionContext);
}

void GBufferPass::prepareAOT(FrameCoordinator* coordinator)
{
    passDataDescriptorLayout = coordinator->manageInFlightDescriptorSetAOT("GBufferPassDataDescriptorSet", { {vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eAllGraphics } });

    coordinator->updateInFlightDescriptorSetAOT("GBufferPassDataDescriptorSet", 0, vk::DescriptorType::eUniformBuffer,[this](GPUFrame* frame) {
        return vk::Buffer(scene->mainView.camera.data.getBufferFor(frame->frameIdx));
    });

    passLevelPipelineLayout = coordinator->backendDevice->createPipelineLayout2({ coordinator->getFrameGlobalDescriptorSetLayout(), passDataDescriptorLayout });
    coordinator->backendDevice->setObjectDebugName(passLevelPipelineLayout, "GBufferPassLevelPipelineLayout");

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

void GBufferPass::prepareIncremental(GPUFrame* frame)
{
    actionContextQueue.clear();
    currentInstanceDescriptorSetIdx = -1;
    if (scene != nullptr && !scene->_dynamicRigidMeshBatch.empty())
    {
        auto view = scene->mainView.camera.data->view;
        auto proj = scene->mainView.camera.data->proj;
        auto vp = proj * view;

        int visibleCount = 0;

        for (int i = 0; i < scene->_dynamicRigidMeshBatch.size(); i++)
        {
            auto& instanceRigidDynamic = scene->_dynamicRigidMeshBatch[i];
            // renderState = vertex input state + pipeline layout + shader + pipeline
            // todo : Here is a big problem. The performance is unpredictable.
            PassActionContext actionContext{};
            actionContext.firstSet = 1;
            actionContext.descriptorSets.push_back("GBufferPassDataDescriptorSet");
            bindRenderState(actionContext, frame, instanceRigidDynamic);
            actionContext.action = [this,i,instanceRigidDynamic](vk::CommandBuffer cmd, uint32_t pipelineIdx) {
                glm::uvec4 meshIdx;
                meshIdx.x = i;
                meshIdx.y = scene->_dynamicRigidMeshBatch.size();
                cmd.pushConstants(graphicsPipelines[pipelineIdx].getPipelineLayout(), vk::ShaderStageFlagBits::eFragment, 0, sizeof(glm::uvec4), &meshIdx);
                //mesh instance know how to bind the geometry buffer, how to draw
                instanceRigidDynamic.drawAll(cmd);
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
            };
            actionContextQueue.push_back(actionContext);
        }
    }
}

void DeferredLightingPass::prepareAOT(FrameCoordinator* coordinator)
{
    auto vs = FullScreenQuadDrawer::getVertexShader(coordinator->backendDevice);
    auto fs = ShaderManager::getInstance().createFragmentShader(coordinator->backendDevice, "deferred.frag");

    auto pipelineLayout = coordinator->backendDevice->createPipelineLayout2({ coordinator->getFrameGlobalDescriptorSetLayout(),passInputDescriptorSetLayout });
    coordinator->backendDevice->setObjectDebugName(pipelineLayout, "DeferredLightingPassPipelineLayout");
    VulkanGraphicsPipelineBuilder builder(coordinator->backendDevice->device, vs, fs,
        FullScreenQuadDrawer::getVertexInputStateInfo(), renderPass,
        pipelineLayout);
    auto pipeline = builder.build();
    coordinator->backendDevice->setObjectDebugName(pipeline.getPipeline(), "DeferredLightingPassPipeline");
    graphicsPipelines.push_back(pipeline);
    PassActionContext actionContext{};
    actionContext.pipelineIdx = 0;
    actionContext.firstSet = 1;
    actionContext.descriptorSets = { "DeferredLightingPassInputDescriptorSet" };
    actionContext.action = [](vk::CommandBuffer cmd, uint32_t pipelineIdx) {FullScreenQuadDrawer::draw(cmd); };
    actionContextQueue.push_back(actionContext);
}

void PostProcessPass::prepareAOT(FrameCoordinator* coordinator)
{
    auto vs = FullScreenQuadDrawer::getVertexShader(coordinator->backendDevice);
    auto fs = ShaderManager::getInstance().createFragmentShader(coordinator->backendDevice, "postProcess.frag");

    auto pipelineLayout = coordinator->backendDevice->createPipelineLayout2({ coordinator->getFrameGlobalDescriptorSetLayout(),passInputDescriptorSetLayout });
    coordinator->backendDevice->setObjectDebugName(pipelineLayout, "PostProcessPassPipelineLayout");

    VulkanGraphicsPipelineBuilder builder(coordinator->backendDevice->device, vs, fs,
        FullScreenQuadDrawer::getVertexInputStateInfo(), renderPass,
        pipelineLayout);
    auto pipeline = builder.build();
    coordinator->backendDevice->setObjectDebugName(pipeline.getPipeline(), "PostProcessPassPipeline");
    graphicsPipelines.push_back(pipeline);
    PassActionContext actionContext{};
    actionContext.pipelineIdx = 0;
    actionContext.firstSet = 1;
    actionContext.descriptorSets = { "PostProcessPassInputDescriptorSet" };
    actionContext.action = [](vk::CommandBuffer cmd, uint32_t pipelineIdx) {FullScreenQuadDrawer::draw(cmd); };
    actionContextQueue.push_back(actionContext);
}

void CopyPass::prepareAOT(FrameCoordinator* coordinator)
{
    auto vs = FullScreenQuadDrawer::getVertexShader(coordinator->backendDevice);
    auto fs = ShaderManager::getInstance().createFragmentShader(coordinator->backendDevice, "copy.frag");

    vk::PushConstantRange pushConstant{};
    pushConstant.setOffset(0);
    pushConstant.setSize(sizeof(glm::uvec4));
    pushConstant.setStageFlags(vk::ShaderStageFlagBits::eFragment);

    auto pipelineLayout = coordinator->backendDevice->createPipelineLayout2({ coordinator->getFrameGlobalDescriptorSetLayout(),
                                                                            passInputDescriptorSetLayout }, { pushConstant });

    coordinator->backendDevice->setObjectDebugName(pipelineLayout, "CopyPassPipelineLayout");

    VulkanGraphicsPipelineBuilder builder(coordinator->backendDevice->device, vs, fs,
        FullScreenQuadDrawer::getVertexInputStateInfo(), renderPass,
        pipelineLayout);
    auto pipeline = builder.build();
    coordinator->backendDevice->setObjectDebugName(pipeline.getPipeline(), "CopyPassPipeline");
    graphicsPipelines.push_back(pipeline);
    PassActionContext actionContext{};
    actionContext.pipelineIdx = 0;
    actionContext.firstSet = 1;
    actionContext.descriptorSets = { "CopyPassInputDescriptorSet" };
    actionContext.action = [this](vk::CommandBuffer cmd, uint32_t pipelineIdx) { 
        cmd.pushConstants(this->graphicsPipelines[pipelineIdx].getPipelineLayout(), vk::ShaderStageFlagBits::eFragment, 0, sizeof(glm::uvec4), &this->currentTexIdx);
        FullScreenQuadDrawer::draw(cmd); 
    };
    actionContextQueue.push_back(actionContext);
}

void SelectedMaskPass::prepareAOT(FrameCoordinator* coordinator)
{
    passDataDescriptorLayout = coordinator->manageInFlightDescriptorSetAOT("SelectedMaskPassDataDescriptorSet", { {vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eAllGraphics } });

    coordinator->updateInFlightDescriptorSetAOT("SelectedMaskPassDataDescriptorSet", 0, vk::DescriptorType::eUniformBuffer,[this](GPUFrame* frame) {
        return vk::Buffer(scene->mainView.camera.data.getBufferFor(frame->frameIdx));
    });
}

void SelectedMaskPass::prepareIncremental(GPUFrame* frame)
{
    actionContextQueue.clear();
    for (int i = 0; i < scene->_dynamicRigidMeshBatch.size(); i++)
    {
        auto& instanceRigidDynamic = scene->_dynamicRigidMeshBatch[i];
        if (graphicsPipelines.empty())
        {
            pipelineLayout = frame->backendDevice->createPipelineLayout2({ frame->getFrameGlobalDescriptorSetLayout(),passDataDescriptorLayout,instanceRigidDynamic.getSetLayout() });
            frame->backendDevice->setObjectDebugName(pipelineLayout, "SelectedMaskPassPipelineLayout");
            auto vs = ShaderManager::getInstance().createVertexShader(frame->backendDevice, "positionOnly.vert");
            auto fs = ShaderManager::getInstance().createFragmentShader(frame->backendDevice, "constantColor.frag");
            VulkanGraphicsPipelineBuilder builder(frame->backendDevice->device, vs, fs,
                instanceRigidDynamic.getPosOnlyVertexInputState().getCreateInfo(), renderPass,
                pipelineLayout);
            builder.addDynamicState(vk::DynamicState::eVertexInputBindingStride);
            auto pipeline = builder.build();
            frame->backendDevice->setObjectDebugName(pipeline.getPipeline(), "SelectedMaskPassPipeline");
            graphicsPipelines.push_back(pipeline);
        }
        if (instanceRigidDynamic.updateCurrentMask(1, frame->backendDevice) > 0)
        {
            PassActionContext actionContext{};
            actionContext.pipelineIdx = 0;
            actionContext.firstSet = 1;
            actionContext.descriptorSets = { "SelectedMaskPassDataDescriptorSet" , instanceRigidDynamic.getDescriptorSet() };
            actionContext.action = [instanceRigidDynamic, frame](vk::CommandBuffer cmd, uint32_t pipelineIdx){
                instanceRigidDynamic.drawAllPosOnlyMasked(cmd, frame->backendDevice->getDLD());
            };
            actionContextQueue.push_back(actionContext);
        }
    }
}

void WireFramePass::prepareAOT(FrameCoordinator* coordinator)
{
    passDataDescriptorLayout = coordinator->manageInFlightDescriptorSetAOT("WireFramePassDataDescriptorSet", { {vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eAllGraphics } });

    coordinator->updateInFlightDescriptorSetAOT("WireFramePassDataDescriptorSet", 0, vk::DescriptorType::eUniformBuffer,[this](GPUFrame* frame) {
        return vk::Buffer(scene->mainView.camera.data.getBufferFor(frame->frameIdx));
        });
}

void WireFramePass::prepareIncremental(GPUFrame* frame)
{
    actionContextQueue.clear();
    for (int i = 0; i < scene->_dynamicRigidMeshBatch.size(); i++)
    {
        auto& instanceRigidDynamic = scene->_dynamicRigidMeshBatch[i];
        if (graphicsPipelines.empty())
        {
            pipelineLayout = frame->backendDevice->createPipelineLayout2({ frame->getFrameGlobalDescriptorSetLayout(),passDataDescriptorLayout,instanceRigidDynamic.getSetLayout()});
            frame->backendDevice->setObjectDebugName(pipelineLayout, "WireFramePassPipelineLayout");
            auto vs = ShaderManager::getInstance().createVertexShader(frame->backendDevice, "positionOnly.vert");
            auto fs = ShaderManager::getInstance().createFragmentShader(frame->backendDevice, "constantColor.frag");

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
                pipelineLayout, rasterInfo, depthStencilInfo);
            builder.addDynamicState(vk::DynamicState::eVertexInputBindingStride);
            auto pipeline = builder.build();
            frame->backendDevice->setObjectDebugName(pipeline.getPipeline(), "WireFramePassPipeline");
            graphicsPipelines.push_back(pipeline);
        }
        PassActionContext actionContext{};
        actionContext.pipelineIdx = 0;
        actionContext.firstSet = 1;
        actionContext.descriptorSets = { "WireFramePassDataDescriptorSet" , instanceRigidDynamic.getDescriptorSet() };
        actionContext.action = [instanceRigidDynamic, frame](vk::CommandBuffer cmd, uint32_t pipelineIdx) {
            instanceRigidDynamic.drawAllPosOnly(cmd, frame->backendDevice->getDLD());
         };
        actionContextQueue.push_back(actionContext);
    }
}

void OutlinePass::prepareAOT(FrameCoordinator* coordinator)
{
    auto vs = FullScreenQuadDrawer::getVertexShader(coordinator->backendDevice);
    auto fs = ShaderManager::getInstance().createFragmentShader(coordinator->backendDevice, "outline.frag");

    auto pipelineLayout = coordinator->backendDevice->createPipelineLayout2({ coordinator->getFrameGlobalDescriptorSetLayout(),passInputDescriptorSetLayout});
    coordinator->backendDevice->setObjectDebugName(pipelineLayout, "OutlinePassPipelineLayout");

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

    VulkanGraphicsPipelineBuilder builder(coordinator->backendDevice->device, vs, fs,
        FullScreenQuadDrawer::getVertexInputStateInfo(), renderPass,
        pipelineLayout, colorBlendInfo);
    auto pipeline = builder.build();
    coordinator->backendDevice->setObjectDebugName(pipeline.getPipeline(), "OutlinePassPipeline");
    graphicsPipelines.push_back(pipeline);
    PassActionContext actionContext{};
    actionContext.pipelineIdx = 0;
    actionContext.firstSet = 1;
    actionContext.descriptorSets = { "OutlinePassInputDescriptorSet" };
    actionContext.action = [](vk::CommandBuffer cmd, uint32_t pipelineIdx) {FullScreenQuadDrawer::draw(cmd); };
    actionContextQueue.push_back(actionContext);
}

void ObjectPickPass::prepareAOT(FrameCoordinator* coordinator)
{
    auto resBuffer = coordinator->backendDevice->allocateObservedBufferPull<glm::uvec4>(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    if (!resBuffer)
    {
        throw std::runtime_error("Failed to create objectIDBuffer");
    }
    objectIDBuffer = resBuffer.value();

    passDataDescriptorLayout = coordinator->manageInFlightDescriptorSetAOT("ObjectPickPassDataDescriptorSet", { {vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute }});
    coordinator->updateInFlightDescriptorSetAOT("ObjectPickPassDataDescriptorSet", 0, vk::DescriptorType::eStorageBuffer,[this](GPUFrame* frame) {
        return vk::Buffer(objectIDBuffer.getBuffer());
    });

    pipelineLayout = coordinator->backendDevice->createPipelineLayout2({ coordinator->getFrameGlobalDescriptorSetLayout(),
        passInputDescriptorSetLayout, passDataDescriptorLayout });

    auto cs = ShaderManager::getInstance().createComputeShader(coordinator->backendDevice, "objectPick.comp");
    vk::ComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.setLayout(pipelineLayout);
    pipelineInfo.setStage(cs->getStageCreateInfo());
    VulkanComputePipelineBuilder builder(coordinator->backendDevice->device, cs, pipelineLayout);
    auto pipeline = builder.build();
    computePipelines.push_back(pipeline);
    coordinator->backendDevice->setObjectDebugName(pipelineLayout, "ObjectPickPassPipelineLayout");
    coordinator->backendDevice->setObjectDebugName(pipeline.getPipeline(), "ObjectPickPassPipeline");

    PassActionContext actionContext{};
    actionContext.pipelineIdx = 0;
    actionContext.firstSet = 1;
    actionContext.descriptorSets = {"ObjectPickPassInputDescriptorSet","ObjectPickPassDataDescriptorSet" };
    actionContext.action = [this](vk::CommandBuffer cmd, uint32_t pipelineIdx) {
        cmd.dispatch(1, 1, 1);
        scene->selectedDynamicRigidMeshID.x = objectIDBuffer->x;
        scene->selectedDynamicRigidMeshID.y = objectIDBuffer->y;
    };
    actionContextQueue.push_back(actionContext);
}

void GUIPass::prepareAOT(FrameCoordinator* coordinator)
{
    gui->renderInit(renderPass);
    PassActionContext actionContext{};
    actionContext.pipelineIdx = -1;
    actionContext.firstSet = 1;
    actionContext.descriptorSets = {};
    actionContext.action = [this](vk::CommandBuffer cmd, uint32_t pipelineIdx) {
        gui->render(cmd);
    };
    actionContextQueue.push_back(actionContext);
}
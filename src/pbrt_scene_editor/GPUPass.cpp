//
// Created by 王泽远 on 2024/1/18.
//

#include "GPUPass.h"
#include <stack>
#include "window.h"

bool isDepthStencilFormat(vk::Format format)
{
        return format == vk::Format::eD32Sfloat ||
        format == vk::Format::eD16Unorm ||
        format == vk::Format::eD16UnormS8Uint ||
        format == vk::Format::eD24UnormS8Uint ||
        format == vk::Format::eD32SfloatS8Uint;
};

void PassTextureDescription::resolveBarriers(GPUFrame* frame)
{
    // For the pass who firstly attach an image, we don't need sync but we may need to transition the layout
    std::vector<PassResourceAccessInfo> activeAccessList;
    activeAccessList.reserve(accessList.size());
    for (const auto & access : accessList)
    {
        if (access.pass->enabled)
        {
            activeAccessList.push_back(access);
        }
    }

    for (int i = 1; i < activeAccessList.size(); i++)
    {
        auto preAccessType = activeAccessList[i - 1].accessType;
        auto postAccessType = activeAccessList[i].accessType;

        bool has_hazard = false;
        
        has_hazard |= (std::holds_alternative<PassResourceAccess::Read>(preAccessType) && std::holds_alternative<PassResourceAccess::Write>(postAccessType));
        has_hazard |= (std::holds_alternative<PassResourceAccess::Read>(preAccessType) && std::holds_alternative<PassResourceAccess::RenderTarget>(postAccessType));
        has_hazard |= (std::holds_alternative<PassResourceAccess::Read>(preAccessType) && std::holds_alternative<PassResourceAccess::Sample>(postAccessType)); // layout transition need

        has_hazard |= (std::holds_alternative<PassResourceAccess::Sample>(preAccessType) && std::holds_alternative<PassResourceAccess::Write>(postAccessType));
        has_hazard |= (std::holds_alternative<PassResourceAccess::Sample>(preAccessType) && std::holds_alternative<PassResourceAccess::RenderTarget>(postAccessType));
        has_hazard |= (std::holds_alternative<PassResourceAccess::Sample>(preAccessType) && std::holds_alternative<PassResourceAccess::Read>(postAccessType)); // layout transition need

        has_hazard |= (std::holds_alternative<PassResourceAccess::Write>(preAccessType) && std::holds_alternative<PassResourceAccess::Write>(postAccessType));
        has_hazard |= (std::holds_alternative<PassResourceAccess::Write>(preAccessType) && std::holds_alternative<PassResourceAccess::RenderTarget>(postAccessType));
        has_hazard |= (std::holds_alternative<PassResourceAccess::Write>(preAccessType) && std::holds_alternative<PassResourceAccess::Read>(postAccessType));
        has_hazard |= (std::holds_alternative<PassResourceAccess::Write>(preAccessType) && std::holds_alternative<PassResourceAccess::Sample>(postAccessType));

        has_hazard |= (std::holds_alternative<PassResourceAccess::RenderTarget>(preAccessType) && std::holds_alternative<PassResourceAccess::Write>(postAccessType));
        has_hazard |= (std::holds_alternative<PassResourceAccess::RenderTarget>(preAccessType) && std::holds_alternative<PassResourceAccess::RenderTarget>(postAccessType));
        has_hazard |= (std::holds_alternative<PassResourceAccess::RenderTarget>(preAccessType) && std::holds_alternative<PassResourceAccess::Read>(postAccessType));
        has_hazard |= (std::holds_alternative<PassResourceAccess::RenderTarget>(preAccessType) && std::holds_alternative<PassResourceAccess::Sample>(postAccessType));

        if (has_hazard)
        {
            vk::ImageMemoryBarrier2 imb{};
            imb.setImage(frame->getBackingImage(name));
            vk::ImageSubresourceRange subresourceRange{};
            subresourceRange.setBaseMipLevel(0);
            subresourceRange.setLevelCount(1);
            subresourceRange.setBaseArrayLayer(0);
            subresourceRange.setLayerCount(1);
            if (isDepthStencilFormat(format))
            {
                subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eDepth);
            }
            else {
                subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor);
            }
            imb.setSubresourceRange(subresourceRange);
            imb.setSrcQueueFamilyIndex(vk::QueueFamilyIgnored);
            imb.setDstQueueFamilyIndex(vk::QueueFamilyIgnored);
            activeAccessList[i - 1].pass->fillImgBarrierInfo(preAccessType, this, imb.srcStageMask, imb.srcAccessMask, imb.oldLayout);
            activeAccessList[i].pass->fillImgBarrierInfo(postAccessType, this, imb.dstStageMask, imb.dstAccessMask, imb.newLayout);
            activeAccessList[i].pass->imageMemoryBarriers.emplace_back(imb);
        }
    }
}

void PassBufferDescription::resolveBarriers(GPUFrame* frame)
{
    std::vector<PassResourceAccessInfo> activeAccessList;
    activeAccessList.reserve(accessList.size());
    for (const auto & access : accessList)
    {
        if (access.pass->enabled)
        {
            activeAccessList.push_back(access);
        }
    }
    for (int i = 1; i < activeAccessList.size(); i++)
    {
        auto preAccessType = activeAccessList[i - 1].accessType;
        auto postAccessType = activeAccessList[i].accessType;

        bool has_hazard = false;

        has_hazard |= (std::holds_alternative<PassResourceAccess::Read>(preAccessType) && std::holds_alternative<PassResourceAccess::Write>(postAccessType));

        has_hazard |= (std::holds_alternative<PassResourceAccess::Write>(preAccessType) && std::holds_alternative<PassResourceAccess::Write>(postAccessType));
        has_hazard |= (std::holds_alternative<PassResourceAccess::Write>(preAccessType) && std::holds_alternative<PassResourceAccess::Read>(postAccessType));

        if (has_hazard)
        {
            
        }
    }
}

vk::PipelineLayoutCreateInfo concatLayoutCreateInfo(vk::PipelineLayoutCreateInfo baseInfo, vk::PipelineLayoutCreateInfo derivedInfo)
{
    std::vector<vk::DescriptorSetLayout> setLayouts;
    setLayouts.reserve(baseInfo.setLayoutCount + derivedInfo.setLayoutCount);
    for(int i = 0; i < baseInfo.setLayoutCount; i ++)
    {
        setLayouts.push_back(baseInfo.pSetLayouts[i]);
    }
    for(int i = 0; i < baseInfo.setLayoutCount; i ++)
    {
        setLayouts.push_back(derivedInfo.pSetLayouts[i]);
    }

    vk::PipelineLayoutCreateInfo newInfo{};
    newInfo.setSetLayouts(setLayouts);
    return newInfo;
}

void GPURasterizedPass::beginPass(vk::CommandBuffer cmdBuf) {
    vk::RenderPassBeginInfo beginInfo{};
    beginInfo.setRenderPass(renderPass);
    beginInfo.setFramebuffer(frameBuffer);
    vk::Rect2D renderArea{};
    renderArea.offset.x = 0;
    renderArea.offset.y = 0;
    renderArea.extent.width = framebufferCreateInfo.width;
    renderArea.extent.height = framebufferCreateInfo.height;
    beginInfo.setRenderArea(renderArea);
    //todo hard-code for now
    std::vector<vk::ClearValue> clearValues;
    for (const auto& attachmentDesc : attachmentDescriptions)
    {
        vk::ClearValue clearValue;
        if (isDepthStencilFormat(attachmentDesc.format))
        {
            vk::ClearDepthStencilValue value{};
            value.depth = 1.0;
            value.stencil = 0.0;
            clearValue.setDepthStencil(value);
        }
        else {
            vk::ClearColorValue value{};
            value.setFloat32({0.0,0.0,0.0,0.0});
            clearValue.setColor(value);
        }
        clearValues.emplace_back(clearValue);
    }
    beginInfo.setClearValues(clearValues);
    cmdBuf.beginRenderPass(beginInfo,vk::SubpassContents::eInline);
}

void GPURasterizedPass::endPass(vk::CommandBuffer cmdBuf) {
    cmdBuf.endRenderPass();
}

void GPURasterizedPass::buildRenderPass(GPUFrame* frame) {

    attachmentDescriptions.clear();
    for (const auto& renderTarget : renderTargets)
    {
        vk::AttachmentDescription attachment{};
        attachment.setFormat(renderTarget.texture->format);
        attachment.setSamples(vk::SampleCountFlagBits::e1);
        if (isDepthStencilFormat(attachment.format))
        {
            attachment.setInitialLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
            attachment.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
        }
        else {
            attachment.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal);
            attachment.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal);
        }
        attachment.setLoadOp(renderTarget.loadOp);
        attachment.setStoreOp(renderTarget.storeOp);
        // We assume currently we don't use stencil attachment
        attachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
        attachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
        attachmentDescriptions.emplace_back(attachment);
    }

    renderPassCreateInfo.setAttachments(attachmentDescriptions);

    subpassInfo = vk::SubpassDescription{};
    subpassInfo.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);

    colorAttachmentRefs.clear();
    colorAttachmentRefs.reserve(attachmentDescriptions.size());
    depthStencilAttachmentRef = vk::AttachmentReference{};

    for (int i = 0; i < attachmentDescriptions.size(); i++)
    {
        if (isDepthStencilFormat(attachmentDescriptions[i].format))
        {
            depthStencilAttachmentRef.attachment = i;
            //the layout the attachment uses during the subpass.
            depthStencilAttachmentRef.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
        }
        else {
            vk::AttachmentReference colorRef{};
            colorRef.attachment = i;
            colorRef.layout = vk::ImageLayout::eColorAttachmentOptimal;
            colorAttachmentRefs.emplace_back(colorRef);
        }
    }

    subpassInfo.setColorAttachments(colorAttachmentRefs);
    if (depthStencilAttachmentRef.layout != vk::ImageLayout::eUndefined)
    {
        subpassInfo.setPDepthStencilAttachment(&depthStencilAttachmentRef);
    }
    //subpass.setResolveAttachments();
    // We only use one subpass per render pass
    renderPassCreateInfo.setSubpasses(subpassInfo);
    // Since we only have one subpass, we don't care about the dependency ...
    // todo or do we?
    renderPassCreateInfo.setDependencies({});
    renderPass = frame->backendDevice->createRenderPass(renderPassCreateInfo);
    auto passName = this->_name + "RenderPass";
    frame->backendDevice->setObjectDebugName(renderPass, passName.c_str());
}
/*
 * May need recreate when swapChain invalid
 */
void GPURasterizedPass::buildFrameBuffer(GPUFrame* frame) {
    assert(renderPass != VK_NULL_HANDLE);
    framebufferCreateInfo.setRenderPass(renderPass);
    //For now, we assume each attachment has same size
    int width = 0;
    int height = 0;

    std::vector<vk::ImageView> imageViews{};

    for (const auto& renderTarget : renderTargets)
    {
        auto attachmentExtent = renderTarget.texture->getExtent(frame->backendDevice->_swapchain);
        if (width == 0) {
            width = attachmentExtent.width;
        }
        else {
            assert(width == attachmentExtent.width);
        }

        if (height == 0) {
            height = attachmentExtent.height;
        }
        else {
            assert(height == attachmentExtent.height);
        }
        imageViews.push_back(frame->getBackingImageView(this->_name + "::" + renderTarget.texture->name));
    }

    framebufferCreateInfo.setWidth(width);
    framebufferCreateInfo.setHeight(height);
    framebufferCreateInfo.setLayers(1);
    framebufferCreateInfo.setAttachments(imageViews);
    frameBuffer = frame->backendDevice->createFramebuffer(framebufferCreateInfo);
    auto fbName = this->_name + "FrameBuffer";
    frame->backendDevice->setObjectDebugName(frameBuffer, fbName.c_str());
}

GPUFrame::GPUFrame(int threadsNum, const std::shared_ptr<DeviceExtended> &backendDevice) : workThreadsNum(threadsNum),backendDevice(backendDevice)
{
    uint32_t mainQueueFamilyIdx = backendDevice->get_queue_index(vkb::QueueType::graphics).value();
    vk::CommandPoolCreateInfo commandPoolInfo{};
    commandPoolInfo.queueFamilyIndex = mainQueueFamilyIdx;
    commandPoolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

    for (int i = 0; i < workThreadsNum; i++)
    {
        CommandPoolExtended pool(backendDevice,backendDevice->createCommandPool(commandPoolInfo));
        perThreadMainCommandAllocators.emplace_back(pool);
    }

    auto swapChainFormat = static_cast<vk::Format>(backendDevice->_swapchain.image_format);
    auto swapChainExtent = backendDevice->_swapchain.extent;

    swapchainTextureDesc = std::make_unique<PassTexture>("SwapchainImage", swapChainFormat, PassTextureExtent::SwapchainRelative(1, 1));

    //todo multiple queues stuffs ...
    std::array<vk::DescriptorSetLayoutBinding,1> bindings;

    for(auto & binding : bindings)
    {
        binding.setBinding(0);
        binding.setStageFlags(vk::ShaderStageFlagBits::eAll);
        binding.setDescriptorType(vk::DescriptorType::eUniformBuffer);
        binding.setDescriptorCount(1);
    }

    vk::DescriptorSetLayoutCreateInfo layoutCreateInfo;
    layoutCreateInfo.setBindings(bindings);
    _frameGlobalDescriptorSetLayout = backendDevice->createDescriptorSetLayout(layoutCreateInfo);
    backendDevice->setObjectDebugName(_frameGlobalDescriptorSetLayout,"FrameGlobalDescriptorSetLayout");

    vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.setSetLayouts(_frameGlobalDescriptorSetLayout);
    _frameLevelPipelineLayout = backendDevice->createPipelineLayout(pipelineLayoutCreateInfo);
    backendDevice->setObjectDebugName(_frameLevelPipelineLayout, "FrameLevelPipelineLayout");

    vk::DescriptorPoolCreateInfo descriptorPoolInfo{};
    descriptorPoolInfo.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);
    descriptorPoolInfo.setMaxSets(100);
    std::array<vk::DescriptorPoolSize,1> poolSizes;
    for(auto & poolSz : poolSizes)
    {
        poolSz.setType(vk::DescriptorType::eUniformBuffer);
        poolSz.setDescriptorCount(1);
    }
    descriptorPoolInfo.setPoolSizes(poolSizes);
    _frameGlobalDescriptorSetPool = backendDevice->createDescriptorPool(descriptorPoolInfo);
    vk::DescriptorSetAllocateInfo allocateInfo{};
    allocateInfo.setSetLayouts(_frameGlobalDescriptorSetLayout);
    allocateInfo.setDescriptorPool(_frameGlobalDescriptorSetPool);
    allocateInfo.setDescriptorSetCount(1);
    _frameGlobalDescriptorSet = backendDevice->allocateDescriptorSets(allocateInfo)[0];
    backendDevice->setObjectDebugName(_frameGlobalDescriptorSet, "FrameGlobalDescriptorSet");

    auto buf = backendDevice->allocateObservedBufferPull<frameGlobalData>(VkBufferUsageFlagBits::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT).value();
    _frameGlobalDataBuffer = buf;

    //Resetting a descriptor pool recycles all of the resources from all of the descriptor sets
    // allocated from the descriptor pool back to the descriptor pool, and the descriptor sets are implicitly freed.

    //Once all pending uses have completed, it is legal to update and reuse a descriptor set.
    //https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkCmdBindDescriptorSets.html
    vk::WriteDescriptorSet write{};
    write.setDescriptorType(vk::DescriptorType::eUniformBuffer);
    write.setDescriptorCount(1);
    write.setDstBinding(0);
    write.setDstSet(_frameGlobalDescriptorSet);
    vk::DescriptorBufferInfo bufferInfo;
    bufferInfo.setOffset(0);
    bufferInfo.setBuffer(_frameGlobalDataBuffer.getBuffer());
    bufferInfo.setRange(vk::WholeSize);
    write.setBufferInfo(bufferInfo);
    backendDevice->updateDescriptorSets(write,{});
}

void GPUFrame::allocateResources()
{
    for (auto& texture : textureDescriptions)
    {
        auto backingImage = createBackingImage(texture.get());
        assert(backingImage.has_value());
        backingImages.emplace(texture->name, backingImage.value());

        for (auto & accessInfo : texture->accessList)
        {
           backingImageViews.emplace(accessInfo.pass->_name+"::"+texture->name, createBackingImageView(texture.get(), accessInfo.accessType, getBackingImage(texture->name)));
        }
    }

    for (auto& accessInfo : swapchainTextureDesc->accessList)
    {
        backingImageViews.emplace(accessInfo.pass->_name + "::" + swapchainTextureDesc->name, backendDevice->_swapchain.get_image_views().value()[frameIdx]);
    }

    //create default sampler
    vk::SamplerCreateInfo defaultSamplerInfo{};
    defaultSamplerInfo.setAddressModeU(vk::SamplerAddressMode::eClampToBorder);
    defaultSamplerInfo.setAddressModeV(vk::SamplerAddressMode::eClampToBorder);
    defaultSamplerInfo.setAddressModeW(vk::SamplerAddressMode::eClampToBorder);
    defaultSamplerInfo.setAnisotropyEnable(vk::False);
    defaultSamplerInfo.setMaxAnisotropy(0.0);
    defaultSamplerInfo.setBorderColor(vk::BorderColor::eFloatOpaqueBlack);
    defaultSamplerInfo.setCompareEnable(vk::False);
    defaultSamplerInfo.setCompareOp(vk::CompareOp::eNever);
    defaultSamplerInfo.setMagFilter(vk::Filter::eLinear);
    defaultSamplerInfo.setMinFilter(vk::Filter::eLinear);
    defaultSamplerInfo.setMaxLod(0);
    defaultSamplerInfo.setMinLod(0);
    defaultSamplerInfo.setMipLodBias(0);
    defaultSamplerInfo.setMipmapMode(vk::SamplerMipmapMode::eLinear);

    samplers.emplace_back(backendDevice->createSampler(defaultSamplerInfo));

    for (auto& buffer : bufferDescriptions)
    {
        
    }
}

void GPUFrame::buildBarriers()
{
    for (auto & pass : _allPasses)
    {
        pass->memoryBarriers.clear();
        pass->bufferMemoryBarriers.clear();
        pass->imageMemoryBarriers.clear();
    }

    for (auto& texture : textureDescriptions)
    {
        texture->resolveBarriers(this);
    }

    for (auto& buffer : bufferDescriptions)
    {
        buffer->resolveBarriers(this);
    }
}

void GPUFrame::compileAOT()
{
    sortedIndices.reserve(_allPasses.size());
    for (int i = 0; i < _allPasses.size(); i++)
    {
        sortedIndices.push_back(i);
    }
    allocateResources();
    for (auto& pass : _allPasses)
    {
        if (pass->getType() == GPUPassType::Graphics)
        {
            auto* graphicsPass = static_cast<GPURasterizedPass*>(pass.get());
            graphicsPass->buildRenderPass(this);
            graphicsPass->buildFrameBuffer(this);
        }
    }
    buildBarriers();
    // Manage pass input descriptorSet
    for (auto& pass : _allPasses)
    {
        managePassInputDescriptorSet(pass.get());
    }

    // Pass ahead of time preparation
    for (auto& pass : _allPasses)
    {
        pass->prepareAOT(this);
    }

    prepareDescriptorSetsAOT();

    Window::registerCursorPosCallback([this](double xPos, double yPos) {
        this->x = xPos;
        this->y = yPos;
        });

    backendDevice->_swapchain.registerRecreateCallback([this](auto) {
        this->update(Event::SWAPCHAIN_RESIZE);
        });
}

void GPUFrame::disablePass(const std::string& passName)
{
    auto* pass = getPass(passName);
    if (pass->enabled)
    {
        pass->enabled = false;
        needToRebuildBarriers = true;
    }
}

void GPUFrame::enablePass(const std::string& passName)
{
    auto* pass = getPass(passName);
    if (!pass->enabled)
    {
        pass->enabled = true;
        needToRebuildBarriers = true;
    }
}

void GPUFrame::update(GPUFrame::Event event) {
    if (event == Event::SWAPCHAIN_RESIZE)
    {
        // We need to rebuild all passes whose framebuffer's size changes
        std::vector<GPURasterizedPass*> needRebuilds;
        for (const auto& access : swapchainTextureDesc->accessList)
        {
            backingImageViews[access.pass->_name + "::" + swapchainTextureDesc->name] = backendDevice->_swapchain.get_image_views().value()[frameIdx];
            if (std::holds_alternative<PassResourceAccess::RenderTarget>(access.accessType))
            {
                assert(access.pass->getType() == GPUPassType::Graphics);
                needRebuilds.push_back(static_cast<GPURasterizedPass*>(access.pass));
            }
        }
        for (const auto& texture : textureDescriptions)
        {
            if (std::holds_alternative<PassTextureExtent::SwapchainRelative>(texture->extent))
            {
                auto newImg = createBackingImage(texture.get());
                assert(newImg.has_value());
                auto oldImg = backingImages[texture->name];
                backingImages[texture->name] = newImg.value();
                backendDevice->deAllocateImage(oldImg.image, oldImg.allocation);
                for (const auto& access : texture->accessList)
                {
                    auto newImgView = createBackingImageView(texture.get(), access.accessType, getBackingImage(texture->name));
                    auto oldImgView = backingImageViews[access.pass->_name + "::" + texture->name];
                    backendDevice->destroy(oldImgView);
                    backingImageViews[access.pass->_name + "::" + texture->name] = newImgView;
                    if (std::holds_alternative<PassResourceAccess::RenderTarget>(access.accessType))
                    {
                        assert(access.pass->getType() == GPUPassType::Graphics);
                        needRebuilds.push_back(static_cast<GPURasterizedPass*>(access.pass));
                    }
                }
            }
        }
        // Remove duplicate elements
        std::sort(needRebuilds.begin(), needRebuilds.end());
        auto last = std::unique(needRebuilds.begin(), needRebuilds.end());
        needRebuilds.erase(last, needRebuilds.end());

        for (int i = 0; i < needRebuilds.size(); i++)
        {
            backendDevice->destroy(needRebuilds[i]->frameBuffer);
            needRebuilds[i]->buildFrameBuffer(this);
        }

        // Update all descriptorsets which ref to an updated attachment/texture
        std::vector<vk::DescriptorImageInfo> imgInfos;
        std::vector<vk::WriteDescriptorSet> writes;

        for (const auto& texture : textureDescriptions)
        {
            if (std::holds_alternative<PassTextureExtent::SwapchainRelative>(texture->extent))
            {
                for (const auto& access : texture->accessList)
                {
                    auto* pass = access.pass;
                    auto textureId = pass->_name + "::" + texture->name;
                    if (std::holds_alternative<PassResourceAccess::Read>(access.accessType))
                    {
                        vk::WriteDescriptorSet write;
                        write.setDstSet(getManagedDescriptorSet(pass->_name + "InputDescriptorSet"));
                        auto dstBinding = 0;
                        for (const auto input : pass->reads)
                        {
                            if (input->name == texture->name)
                            {
                                break;
                            }
                            dstBinding++;
                        }
                        write.setDstBinding(dstBinding);
                        write.setDescriptorType(vk::DescriptorType::eStorageImage);
                        write.setDescriptorCount(1);
                        vk::DescriptorImageInfo imgInfo{};
                        imgInfo.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
                        imgInfo.setImageView(getBackingImageView(textureId));
                        imgInfos.push_back(imgInfo);
                        writes.push_back(write);
                    }

                    if (std::holds_alternative<PassResourceAccess::Write>(access.accessType))
                    {
                        vk::WriteDescriptorSet write;
                        write.setDstSet(getManagedDescriptorSet(pass->_name + "InputDescriptorSet"));
                        auto dstBinding = pass->reads.size();
                        for (const auto input : pass->writes)
                        {
                            if (input->name == texture->name)
                            {
                                break;
                            }
                            dstBinding++;
                        }
                        write.setDstBinding(dstBinding++);
                        write.setDescriptorType(vk::DescriptorType::eStorageImage);
                        write.setDescriptorCount(1);
                        vk::DescriptorImageInfo imgInfo{};
                        imgInfo.setImageLayout(vk::ImageLayout::eGeneral);
                        imgInfo.setImageView(getBackingImageView(textureId));
                        imgInfos.push_back(imgInfo);
                        writes.push_back(write);
                    }

                    if (std::holds_alternative<PassResourceAccess::Sample>(access.accessType))
                    {
                        vk::WriteDescriptorSet write;
                        write.setDstSet(getManagedDescriptorSet(access.pass->_name + "InputDescriptorSet"));
                        auto dstBinding = pass->reads.size() + pass->writes.size();
                        for (const auto input : pass->samples)
                        {
                            if (input->name == texture->name)
                            {
                                break;
                            }
                            dstBinding++;
                        }
                        write.setDstBinding(dstBinding);
                        write.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
                        write.setDescriptorCount(1);
                        vk::DescriptorImageInfo imgInfo{};
                        imgInfo.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
                        imgInfo.setSampler(samplers[0]);
                        imgInfo.setImageView(getBackingImageView(textureId));
                        imgInfos.push_back(imgInfo);
                        writes.push_back(write);
                    }
                }
            }
        }

        for (int i = 0; i < writes.size(); i++)
        {
            writes[i].setPImageInfo(&imgInfos[i]);
        }

        backendDevice->updateDescriptorSets(writes, {});
        needToRebuildBarriers = true;
    }
}

vk::CommandBuffer GPUFrame::recordMainQueueCommands() {

    if (needToRebuildBarriers)
    {
        buildBarriers();
        needToRebuildBarriers = false;
    }

    _frameGlobalDataBuffer->time.x = static_cast<float>(glfwGetTime());
    _frameGlobalDataBuffer->cursorPos.x = this->x;
    _frameGlobalDataBuffer->cursorPos.y = this->y;

    //Resetting a descriptor pool recycles all of the resources from all of the descriptor sets
    // allocated from the descriptor pool back to the descriptor pool, and the descriptor sets are implicitly freed.

    //Once all pending uses have completed, it is legal to update and reuse a descriptor set.
    //https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkCmdBindDescriptorSets.html
    backendDevice->updateDescriptorSetUniformBuffer(_frameGlobalDescriptorSet, 0, _frameGlobalDataBuffer.getBuffer());

    for(auto & allocator : perThreadMainCommandAllocators)
    {
        allocator.reset();
    }

    /*
     * Multi-threading passes recording.
     *
     * Note: as for the command synchronization problem, there is no different
     * whether you record commands into one command buffer or separate them into many.
     * No matter what you need to explicitly do the synchronization job.(By using pipeline barriers).
     *
     * Pipeline barrier will affect all commands that have been submitted to the queue, and commands would be submitted
     * in to the same queue, no matter whether there are recorded into same command buffer or not.
     *
     * All in all, the only two reason we use multiple command buffers is 1. we want to serve different type of
     * operation(which doesn't make difference if we only use omnipotent main queue)
     * 2. we cannot operate on single command buffer in parallel.
     */
    int threadId = 0;
    auto & cmdAllocator = perThreadMainCommandAllocators[threadId];
    auto cmdPrimary = cmdAllocator.getOrAllocateNextPrimary();

    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse);
    cmdPrimary.begin(beginInfo);
    vk::DebugUtilsLabelEXT frameLabel{};
    auto frameLabelName = std::string("Frame ").append(std::to_string(frameIdx));
    frameLabel.setPLabelName(frameLabelName.c_str());
    cmdPrimary.beginDebugUtilsLabelEXT(frameLabel,backendDevice->getDLD());
    cmdPrimary.setViewport(0, backendDevice->_swapchain.getDefaultViewport());
    cmdPrimary.setScissor(0, backendDevice->_swapchain.getDefaultScissor());

    for(int i = 0 ; i < sortedIndices.size(); i ++)
    {
        auto& pass = _allPasses[sortedIndices[i]];
        if (!pass->enabled)
        {
            continue;
        }
        if (pass->getType() == GPUPassType::Graphics)
        {
            cmdPrimary.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, _frameLevelPipelineLayout, 0, _frameGlobalDescriptorSet, nullptr);
        }
        else {
            cmdPrimary.bindDescriptorSets(vk::PipelineBindPoint::eCompute, _frameLevelPipelineLayout, 0, _frameGlobalDescriptorSet, nullptr);
        }
        pass->prepareIncremental(this);
        pass->insertPipelineBarrier(cmdPrimary);
        vk::DebugUtilsLabelEXT passLabel{};
        passLabel.setPLabelName(pass->_name.c_str());
        cmdPrimary.beginDebugUtilsLabelEXT(passLabel,backendDevice->getDLD());
        pass->record(cmdPrimary,this);
        cmdPrimary.endDebugUtilsLabelEXT(backendDevice->getDLD());
    }
    cmdPrimary.endDebugUtilsLabelEXT(backendDevice->getDLD());
    cmdPrimary.end();
    return cmdPrimary;
}

void GPUFrame::managePassInputDescriptorSet(GPUPass* pass) {
    std::vector<PassResourceDescriptionBase*> readwrites;
    readwrites.reserve(pass->reads.size() + pass->writes.size());
    readwrites.insert(readwrites.end(), pass->reads.begin(), pass->reads.end());
    readwrites.insert(readwrites.end(), pass->writes.begin(), pass->writes.end());
    if (readwrites.empty() && pass->samples.empty()) return;
    std::vector<vk::DescriptorSetLayoutBinding> inputBindings;
    inputBindings.reserve(readwrites.size() + pass->samples.size());
    int imgCount = 0;
    int bufferCount = 0;
    vk::DescriptorSetLayoutBinding binding;
    switch (pass->getType())
    {
    case GPUPassType::Graphics:
        binding.setStageFlags(vk::ShaderStageFlagBits::eAllGraphics);
        break;
    case GPUPassType::Compute:
        binding.setStageFlags(vk::ShaderStageFlagBits::eCompute);
        break;
    default:
        break;
    }
    for (auto * input : readwrites)
    {
        binding.setBinding(inputBindings.size());
        binding.setDescriptorCount(1);
        switch (input->getType()) {
        case PassResourceType::Texture:
            binding.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
            imgCount++;
            break;
        case PassResourceType::Buffer:
            binding.setDescriptorType(vk::DescriptorType::eStorageBuffer);
            bufferCount++;
            break;
        default:
            break;
        }
        inputBindings.emplace_back(binding);
    }

    for (auto* input : pass->samples)
    {
        binding.setBinding(inputBindings.size());
        binding.setDescriptorCount(1);
        binding.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
        imgCount++;
        inputBindings.emplace_back(binding);
    }

    pass->passInputDescriptorSetLayout = manageDescriptorSet(pass->_name + "InputDescriptorSet", inputBindings);

    getManagedDescriptorSet(pass->_name + "InputDescriptorSet", [this, pass, imgCount, bufferCount](const vk::DescriptorSet& set)
        {
            std::vector<vk::WriteDescriptorSet> writeDescriptorSets;
            std::vector<vk::DescriptorImageInfo> imgInfos;
            std::vector<vk::DescriptorBufferInfo> bufferInfos;
            writeDescriptorSets.reserve(pass->reads.size() + pass->writes.size() + pass->samples.size());
            imgInfos.reserve(imgCount);
            bufferInfos.reserve(bufferCount);

            for (auto * input : pass->reads)
            {
                vk::WriteDescriptorSet write;
                write.setDstSet(set);
                write.setDstBinding(writeDescriptorSets.size());
                write.setDescriptorCount(1);
                switch (input->getType()) {
                case PassResourceType::Texture:
                {
                    write.setDescriptorType(vk::DescriptorType::eStorageImage);
                    vk::DescriptorImageInfo imgInfo{};
                    imgInfo.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
                    imgInfo.setImageView(getBackingImageView(pass->_name + "::" + input->name));
                    imgInfos.push_back(imgInfo);
                    write.setPImageInfo(&imgInfos[imgInfos.size() - 1]);
                    break;
                }
                case PassResourceType::Buffer: {
                    write.setDescriptorType(vk::DescriptorType::eStorageBuffer);
                    vk::DescriptorBufferInfo bufInfo{};
                    //todo
                    bufferInfos.push_back(bufInfo);
                    write.setPBufferInfo(&bufferInfos[bufferInfos.size() - 1]);
                    break;
                }
                default:
                    break;
                }
                writeDescriptorSets.emplace_back(write);
            }

            for (auto* input : pass->writes)
            {
                vk::WriteDescriptorSet write;
                write.setDstSet(set);
                write.setDstBinding(writeDescriptorSets.size());
                write.setDescriptorCount(1);
                switch (input->getType()) {
                case PassResourceType::Texture:
                {
                    write.setDescriptorType(vk::DescriptorType::eStorageImage);
                    vk::DescriptorImageInfo imgInfo{};
                    imgInfo.setImageLayout(vk::ImageLayout::eGeneral);
                    imgInfo.setImageView(getBackingImageView(pass->_name + "::" + input->name));
                    imgInfos.push_back(imgInfo);
                    write.setPImageInfo(&imgInfos[imgInfos.size() - 1]);
                    break;
                }
                case PassResourceType::Buffer: {
                    write.setDescriptorType(vk::DescriptorType::eStorageBuffer);
                    vk::DescriptorBufferInfo bufInfo{};
                    //todo
                    bufferInfos.push_back(bufInfo);
                    write.setPBufferInfo(&bufferInfos[bufferInfos.size() - 1]);
                    break;
                }
                default:
                    break;
                }
                writeDescriptorSets.emplace_back(write);
            }

            for (auto * input : pass->samples)
            {
                vk::WriteDescriptorSet write;
                write.setDstSet(set);
                write.setDstBinding(writeDescriptorSets.size());
                write.setDescriptorCount(1);
                write.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
                vk::DescriptorImageInfo imgInfo{};
                imgInfo.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
                imgInfo.setSampler(samplers[0]);
                imgInfo.setImageView(getBackingImageView(pass->_name + "::" + input->name));
                imgInfos.push_back(imgInfo);
                write.setPImageInfo(&imgInfos[imgInfos.size() - 1]);
                writeDescriptorSets.emplace_back(write);
            }
            backendDevice->updateDescriptorSets(writeDescriptorSets, {});
        });
}

vk::DescriptorSetLayout GPUFrame::manageDescriptorSet(std::string &&name, const std::vector<vk::DescriptorSetLayoutBinding> &bindings) {
    //Check existing descriptorSet layout
    for(int i = 0 ; i < managedDescriptorSetLayouts.size(); i++)
    {
        auto & layoutRecord = managedDescriptorSetLayouts[i];
        if( layoutRecord.first == bindings)
        {
            layoutRecord.second.emplace_back(name,vk::DescriptorSet{});
            descriptorSetCallbackMap.emplace(name,std::vector<DescriptorSetCallback>{});
            return layoutRecord.first._layout;
        }
    }

    // Need to allocate new descriptorSetLayout Object
    vk::DescriptorSetLayoutCreateInfo layoutCreateInfo{};
    layoutCreateInfo.setBindings(bindings);
    DescriptorSetLayoutExtended newSetLayout(backendDevice->device,layoutCreateInfo);
    managedDescriptorSetLayouts.emplace_back(newSetLayout,std::vector<DescriptorSetRecord>{});
    managedDescriptorSetLayouts.back().second.emplace_back(name,vk::DescriptorSet{});
    descriptorSetCallbackMap.emplace(name,std::vector<DescriptorSetCallback>{});
    return newSetLayout._layout;
}

void GPUFrame::getManagedDescriptorSet(std::string &&setName, const std::function<void(vk::DescriptorSet)> &cb) {
    auto it = descriptorSetCallbackMap.find(setName);
    if(it == descriptorSetCallbackMap.end())
    {
        throw std::runtime_error("Cannot find managed descriptorSet " + setName);
    }
    it->second.emplace_back(cb);
}

void GPUFrame::prepareDescriptorSetsAOT() {
    managedDescriptorPools.reserve(managedDescriptorSetLayouts.size());
    std::vector<vk::DescriptorPoolSize> poolSizes{};

    for(auto & layoutRecord : managedDescriptorSetLayouts) {
        //Create per layout descriptorPool
        vk::DescriptorPoolCreateInfo poolCreateInfo{};
        poolSizes.resize(layoutRecord.first.bindingCount);
        for (int i = 0; i < layoutRecord.first.bindingCount; i++) {
            auto &binding = layoutRecord.first.bindings[i];
            poolSizes[i].setType(binding.descriptorType);
            poolSizes[i].setDescriptorCount(binding.descriptorCount * layoutRecord.second.size());
        }
        poolCreateInfo.setPPoolSizes(poolSizes.data());
        poolCreateInfo.setPoolSizeCount(poolSizes.size());
        poolCreateInfo.setMaxSets(10 * layoutRecord.second.size());
        auto descriptorPool = backendDevice->createDescriptorPool(poolCreateInfo);
        managedDescriptorPools.emplace_back(descriptorPool);
        //allocate DescriptorSets
        vk::DescriptorSetAllocateInfo allocateInfo{};
        allocateInfo.setDescriptorPool(descriptorPool);
        std::vector<vk::DescriptorSetLayout> layouts;
        for(int i = 0; i < layoutRecord.second.size(); i++)
        {
            layouts.push_back(layoutRecord.first._layout);
        }
        allocateInfo.setSetLayouts(layouts);
        allocateInfo.setDescriptorSetCount(layouts.size());
        auto descriptorSets = backendDevice->allocateDescriptorSets(allocateInfo);
        for(int i = 0; i < allocateInfo.descriptorSetCount;i++)
        {
            auto & setRecord = layoutRecord.second[i];
            setRecord.second = descriptorSets[i];
            backendDevice->setObjectDebugName(setRecord.second,setRecord.first.c_str());
            auto it = descriptorSetCallbackMap.find(setRecord.first);
            if(it != descriptorSetCallbackMap.end())
            {
                for(const auto & cb : it->second)
                {
                    cb(setRecord.second);
                }
            }
        }
    }
}

vk::DescriptorSet GPUFrame::getManagedDescriptorSet(std::string && name) const
{
    for(auto & layoutRecord : managedDescriptorSetLayouts)
    {
        for(auto & setRecord : layoutRecord.second)
        {
            if(setRecord.first == name)
                return setRecord.second;
        }
    }

    throw std::runtime_error("Failed to find managed descriptor set " + name);
}
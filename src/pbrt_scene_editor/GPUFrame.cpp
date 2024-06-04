#include "GPUPass.h"
#include "GPUFrame.hpp"
#include "FrameGraph.hpp"
#include "visitor_helper.hpp"

GPUFrame::GPUFrame(int threadsNum,DeviceExtended* backendDevice,FrameCoordinator* coordinator) : workThreadsNum(threadsNum), backendDevice(backendDevice), frameCoordinator(coordinator)
{
    // create synchronize objects
    vk::FenceCreateInfo fenceInfo{};
    fenceInfo.setFlags(vk::FenceCreateFlagBits::eSignaled);
    executingFence = backendDevice->createFence(fenceInfo);
    vk::SemaphoreCreateInfo semaphoreInfo{};
    imageAvailableSemaphore = backendDevice->createSemaphore(semaphoreInfo);
    renderFinishSemaphore = backendDevice->createSemaphore(semaphoreInfo);

    // create command buffers;
    uint32_t mainQueueFamilyIdx = backendDevice->get_queue_index(vkb::QueueType::graphics).value();
    vk::CommandPoolCreateInfo commandPoolInfo{};
    commandPoolInfo.queueFamilyIndex = mainQueueFamilyIdx;
    commandPoolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

    for (int i = 0; i < workThreadsNum; i++)
    {
        CommandPoolExtended pool(backendDevice, backendDevice->createCommandPool(commandPoolInfo));
        perThreadMainCommandAllocators.emplace_back(pool);
    }

    // create frameglobal data
    auto buf = backendDevice->allocateObservedBufferPull<frameGlobalData>(VkBufferUsageFlagBits::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT).value();
    _frameGlobalDataBuffer = buf;
}

vk::DescriptorSetLayout GPUFrame::getFrameGlobalDescriptorSetLayout() const
{
    return frameCoordinator->_frameGlobalDescriptorSetLayout;
}

vk::CommandBuffer GPUFrame::recordMainQueueCommands(uint32_t avaliableSwapChainImageIdx) {

    _frameGlobalDataBuffer->time.x = static_cast<float>(glfwGetTime());
    _frameGlobalDataBuffer->cursorPos.x = this->cursor_x;
    _frameGlobalDataBuffer->cursorPos.y = this->cursor_y;

    //Resetting a descriptor pool recycles all of the resources from all of the descriptor sets
    // allocated from the descriptor pool back to the descriptor pool, and the descriptor sets are implicitly freed.
    //Once all pending uses have completed, it is legal to update and reuse a descriptor set.
    //https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkCmdBindDescriptorSets.html
    backendDevice->updateDescriptorSetUniformBuffer(_frameGlobalDescriptorSet, 0, _frameGlobalDataBuffer.getBuffer());

    for (auto& allocator : perThreadMainCommandAllocators)
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
    auto& cmdAllocator = perThreadMainCommandAllocators[threadId];
    auto cmdPrimary = cmdAllocator.getOrAllocateNextPrimary();

    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse);
    cmdPrimary.begin(beginInfo);
    vk::DebugUtilsLabelEXT frameLabel{};
    auto frameLabelName = std::string("Frame ").append(std::to_string(frameIdx));
    frameLabel.setPLabelName(frameLabelName.c_str());
    cmdPrimary.beginDebugUtilsLabelEXT(frameLabel, backendDevice->getDLD());
    cmdPrimary.setViewport(0, backendDevice->_swapchain.getDefaultViewport());
    cmdPrimary.setScissor(0, backendDevice->_swapchain.getDefaultScissor());

    auto * frameGraph = frameCoordinator->frameGraph;
    frameGraph->buildBarriers(this);
    for (int i = 0; i < frameGraph->sortedIndices.size(); i++)
    {
        auto& pass = frameGraph->_allPasses[frameGraph->sortedIndices[i]];
        if (!pass->is_enabled())
        {
            pass->is_switched_to_enabled = true;
            continue;
        }

        if(pass->is_first_enabled)
        {
            pass->onFirstEnable(this);
            pass->is_first_enabled = false;
        }

        if(pass->is_switched_to_enabled)
        {
            pass->onSwitchToEnable(this);
            pass->is_switched_to_enabled = false;
        }

        pass->onEnable(this);

        if (pass->getType() == GPUPassType::Graphics)
        {
            cmdPrimary.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, frameCoordinator->_frameLevelPipelineLayout, 0, _frameGlobalDescriptorSet, nullptr);
        }
        else {
            cmdPrimary.bindDescriptorSets(vk::PipelineBindPoint::eCompute, frameCoordinator->_frameLevelPipelineLayout, 0, _frameGlobalDescriptorSet, nullptr);
        }
#if __APPLE__
        pass->insertPipelineBarrier(cmdPrimary,backendDevice->getDLD());
#else
        pass->insertPipelineBarrier(cmdPrimary);
#endif
        vk::DebugUtilsLabelEXT passLabel{};
        passLabel.setPLabelName(pass->_name.c_str());
        cmdPrimary.beginDebugUtilsLabelEXT(passLabel, backendDevice->getDLD());
        pass->record(cmdPrimary, this);
        cmdPrimary.endDebugUtilsLabelEXT(backendDevice->getDLD());
    }

    copyPresentToSwapChain(cmdPrimary, avaliableSwapChainImageIdx);

    cmdPrimary.endDebugUtilsLabelEXT(backendDevice->getDLD());
    cmdPrimary.end();
    return cmdPrimary;
}

void GPUFrame::copyPresentToSwapChain(vk::CommandBuffer cmd, uint32_t avaliableSwapChainImageIdx)
{
    vk::Image swapChainImage = backendDevice->_swapchain.get_images().value()[avaliableSwapChainImageIdx];
    auto swapchainExtent = backendDevice->_swapchain.extent;
    //copy present image content to swapChain image
    vk::ImageMemoryBarrier2 imb{};
    imb.image = presentImage.image;
    imb.srcStageMask = presentImage.lastStage;
    imb.srcAccessMask = presentImage.lastAccess;
    imb.oldLayout = presentImage.lastLayout;
    imb.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
    imb.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
    imb.dstStageMask = vk::PipelineStageFlagBits2::eTransfer;
    imb.dstAccessMask = vk::AccessFlagBits2::eTransferRead;
    imb.newLayout = vk::ImageLayout::eTransferSrcOptimal;
    vk::ImageSubresourceRange range{};
    range.aspectMask = vk::ImageAspectFlagBits::eColor;
    range.baseArrayLayer = 0;
    range.layerCount = 1;
    range.baseMipLevel = 0;
    range.levelCount = 1;
    imb.setSubresourceRange(range);

    auto imb2 = imb;
    imb2.image = swapChainImage;
    imb2.srcStageMask = vk::PipelineStageFlagBits2::eNone;
    imb2.srcAccessMask = vk::AccessFlagBits2::eNone;
    imb2.oldLayout = vk::ImageLayout::ePresentSrcKHR;
    imb2.dstStageMask = vk::PipelineStageFlagBits2::eTransfer;
    imb2.dstAccessMask = vk::AccessFlagBits2::eTransferWrite;
    imb2.newLayout = vk::ImageLayout::eTransferDstOptimal;

    vk::DependencyInfo dependencyInfo{};
    std::initializer_list<vk::ImageMemoryBarrier2> imbs{ imb,imb2 };
    dependencyInfo.setImageMemoryBarriers(imbs);
#if __APPLE__
    cmd.pipelineBarrier2KHR(dependencyInfo,backendDevice->getDLD());
#else
    cmd.pipelineBarrier2(dependencyInfo);
#endif

    presentImage.lastStage = vk::PipelineStageFlagBits2::eTransfer;
    presentImage.lastAccess = vk::AccessFlagBits2::eTransferRead;
    presentImage.lastLayout = vk::ImageLayout::eTransferSrcOptimal;

    
    vk::ImageSubresourceLayers subresourceLayers{};
    subresourceLayers.setBaseArrayLayer(0);
    subresourceLayers.setLayerCount(1);
    subresourceLayers.setMipLevel(0);
    subresourceLayers.setAspectMask(vk::ImageAspectFlagBits::eColor);

    vk::ImageCopy copy{};
    copy.setDstSubresource(subresourceLayers);
    copy.setSrcSubresource(subresourceLayers);
    copy.setDstOffset({ 0,0,0 });
    copy.setSrcOffset({ 0,0,0 });
    copy.setExtent({ swapchainExtent.width,swapchainExtent.height,1 });
    cmd.copyImage(presentImage.image, vk::ImageLayout::eTransferSrcOptimal, swapChainImage, vk::ImageLayout::eTransferDstOptimal, copy);

    imb2.srcStageMask = vk::PipelineStageFlagBits2::eTransfer;
    imb2.srcAccessMask = vk::AccessFlagBits2::eTransferWrite;
    imb2.oldLayout = vk::ImageLayout::eTransferDstOptimal;
    imb2.dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
    imb2.dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
    imb2.newLayout = vk::ImageLayout::eColorAttachmentOptimal;

    dependencyInfo.setImageMemoryBarriers(imb2);
#if __APPLE__
    cmd.pipelineBarrier2KHR(dependencyInfo,backendDevice->getDLD());
#else
    cmd.pipelineBarrier2(dependencyInfo);
#endif
}

void GPUFrame::createPresentImage()
{
    auto textureExtent = backendDevice->_swapchain.extent;
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = static_cast<VkFormat>(backendDevice->_swapchain.image_format);
    imageInfo.extent = VkExtent3D{ textureExtent.width,textureExtent.height,1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    auto vmaImage = backendDevice->allocateVMAImage(imageInfo).value();
    auto imageName = "Frame" + std::to_string(frameIdx) + "::" + "PresentImage";

    if (presentVMAImage.image != nullptr)
    {
        backendDevice->deAllocateImage(presentVMAImage.image, presentVMAImage.allocation);
    }

    presentVMAImage = vmaImage;

    presentImage = AccessTrackedImage{ vmaImage.image,vk::AccessFlagBits2::eNone, vk::PipelineStageFlagBits2::eNone,vk::ImageLayout::eUndefined};
    backendDevice->setObjectDebugName(presentImage.image, imageName);

    vk::ImageViewCreateInfo imageViewInfo{};
    imageViewInfo.image = presentImage.image;
    imageViewInfo.viewType = vk::ImageViewType::e2D;
    imageViewInfo.format = static_cast<vk::Format>(imageInfo.format);
    vk::ComponentMapping componentMapping{};
    componentMapping.a = vk::ComponentSwizzle::eA;
    componentMapping.r = vk::ComponentSwizzle::eR;
    componentMapping.g = vk::ComponentSwizzle::eG;
    componentMapping.b = vk::ComponentSwizzle::eB;
    imageViewInfo.components = componentMapping;
    vk::ImageSubresourceRange subresourceRange;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = 1;
    subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = 1;
    imageViewInfo.subresourceRange = subresourceRange;
    if (presentImageView != nullptr)
    {
        backendDevice->destroyImageView(presentImageView);
    }
    presentImageView = backendDevice->createImageView(imageViewInfo);
    backendDevice->setObjectDebugName(presentImageView, imageName);
}

vk::ImageView GPUFrame::getBackingImageView(const std::string& name) const {
    if (name == "PresentImage") return presentImageView;
    return backingImageViews.find(name)->second;
}

vk::Image GPUFrame::getBackingImage(const std::string& name) const {
    return getBackingTrackedImage(name)->image;
}

AccessTrackedImage* GPUFrame::getBackingTrackedImage(const std::string& name) const
{
    if (name == "PresentImage") return &presentImage;
    return backingImages.find(name)->second;
}

vk::DescriptorSet GPUFrame::getManagedDescriptorSet(const std::string& identifier) const
{
    return descriptorSets.find(identifier)->second;
}

void FrameCoordinator::init(DeviceExtended* device, int num_frames_in_flight)
{
    backendDevice = device;
    inFlightframes.clear();

    for (int i = 0; i < num_frames_in_flight; i++)
    {
        auto frame = new GPUFrame(1, device,this);
        frame->frameIdx = i;
        inFlightframes.push_back(frame);
    }

    std::array<vk::DescriptorSetLayoutBinding, 1> bindings;
    for (auto& binding : bindings)
    {
        binding.setBinding(0);
        binding.setStageFlags(vk::ShaderStageFlagBits::eAll);
        binding.setDescriptorType(vk::DescriptorType::eUniformBuffer);
        binding.setDescriptorCount(1);
    }

    vk::DescriptorSetLayoutCreateInfo layoutCreateInfo;
    layoutCreateInfo.setBindings(bindings);
    _frameGlobalDescriptorSetLayout = backendDevice->createDescriptorSetLayout(layoutCreateInfo);
    backendDevice->setObjectDebugName(_frameGlobalDescriptorSetLayout, "FrameGlobalDescriptorSetLayout");

    vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.setSetLayouts(_frameGlobalDescriptorSetLayout);
    _frameLevelPipelineLayout = backendDevice->createPipelineLayout(pipelineLayoutCreateInfo);
    backendDevice->setObjectDebugName(_frameLevelPipelineLayout, "FrameLevelPipelineLayout");

    //Resetting a descriptor pool recycles all of the resources from all of the descriptor sets
    // allocated from the descriptor pool back to the descriptor pool, and the descriptor sets are implicitly freed.
    vk::DescriptorPoolCreateInfo descriptorPoolInfo{};
    descriptorPoolInfo.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);
    descriptorPoolInfo.setMaxSets(10);
    std::array<vk::DescriptorPoolSize, 1> poolSizes;
    for (auto& poolSz : poolSizes)
    {
        poolSz.setType(vk::DescriptorType::eUniformBuffer);
        poolSz.setDescriptorCount(3);
    }
    descriptorPoolInfo.setPoolSizes(poolSizes);
    _frameGlobalDescriptorSetPool = backendDevice->createDescriptorPool(descriptorPoolInfo);

    vk::DescriptorSetAllocateInfo allocateInfo{};
    std::initializer_list<vk::DescriptorSetLayout> layouts{_frameGlobalDescriptorSetLayout, _frameGlobalDescriptorSetLayout, _frameGlobalDescriptorSetLayout };
    allocateInfo.setSetLayouts(layouts);
    allocateInfo.setDescriptorPool(_frameGlobalDescriptorSetPool);
    allocateInfo.setDescriptorSetCount(3);
    auto frameGlobalDescriptorSets = backendDevice->allocateDescriptorSets(allocateInfo);

    std::vector<vk::WriteDescriptorSet> writes{};
    writes.reserve(inFlightframes.size());
    for (auto i = 0; i < inFlightframes.size(); i++)
    {
        auto frame = inFlightframes[i];
        frame->_frameGlobalDescriptorSet = frameGlobalDescriptorSets[i];
        backendDevice->setObjectDebugName(frame->_frameGlobalDescriptorSet, "Frame::"+std::to_string(frame->frameIdx) + "FrameGlobalDescriptorSet");
        //Once all pending uses have completed, it is legal to update and reuse a descriptor set.
        //https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkCmdBindDescriptorSets.html
        vk::WriteDescriptorSet write;
        write.setDescriptorType(vk::DescriptorType::eUniformBuffer);
        write.setDescriptorCount(1);
        write.setDstBinding(0);
        write.setDstSet(frame->_frameGlobalDescriptorSet);

        vk::DescriptorBufferInfo bufferInfo;
        bufferInfo.setOffset(0);
        bufferInfo.setBuffer(frame->_frameGlobalDataBuffer.getBuffer());
        bufferInfo.setRange(vk::WholeSize);
        write.setBufferInfo(bufferInfo);
        writes.push_back(write);
    }

    backendDevice->updateDescriptorSets(writes, {});

    frameGraph = new FrameGraph(static_cast<vk::Format>(backendDevice->_swapchain.image_format));

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

    Window::registerCursorPosCallback([this](double xPos, double yPos) {
        inFlightframes[this->current_frame_idx]->cursor_x = xPos;
        inFlightframes[this->current_frame_idx]->cursor_y = yPos;
    });

    device->_swapchain.registerRecreateCallback([this](SwapchainExtended* newSwapChain) {
        update(Event::SWAPCHAIN_RESIZE);
    });
}

VMAImage FrameCoordinator::createVMAImage(FrameGraphTextureDescription* textureDesc) const
{
    auto textureExtent = textureDesc->getExtent(backendDevice->_swapchain);
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = static_cast<VkFormat>(textureDesc->format);
    imageInfo.extent = VkExtent3D{ textureExtent.width,textureExtent.height,1 };
    if (textureDesc->need_mipmap())
    {
        imageInfo.mipLevels = 1;
    }
    else {
        imageInfo.mipLevels = 1;
    }
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    if (textureDesc->need_sample())
    {
        imageInfo.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    }
    if (textureDesc->use_as_attachment())
    {
        if (VulkanUtil::isDepthStencilFormat(textureDesc->format))
        {
            imageInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        }
        else {
            imageInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }
    }
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    //initialLayout must be VK_IMAGE_LAYOUT_UNDEFINED or VK_IMAGE_LAYOUT_PREINITIALIZED
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    return backendDevice->allocateVMAImage(imageInfo).value();
}

vk::ImageView FrameCoordinator::createVKImageView(FrameGraphTextureDescription* textureDesc, const FrameGraphResourceAccessInfo& accessInfo, vk::Image image) const
{
    vk::ImageViewCreateInfo imageViewInfo{};
    imageViewInfo.viewType = vk::ImageViewType::e2D;
    imageViewInfo.format = textureDesc->format;
    vk::ComponentMapping componentMapping{};
    componentMapping.a = vk::ComponentSwizzle::eA;
    componentMapping.r = vk::ComponentSwizzle::eR;
    componentMapping.g = vk::ComponentSwizzle::eG;
    componentMapping.b = vk::ComponentSwizzle::eB;
    imageViewInfo.components = componentMapping;
    vk::ImageSubresourceRange subresourceRange;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = 1;

    if (VulkanUtil::isDepthStencilFormat(textureDesc->format))
    {
        subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
    }
    else {
        subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    }
    if (std::holds_alternative<FrameGraphResourceAccess::Sample>(accessInfo.accessType))
    {
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = 1;
    }
    else if (std::holds_alternative<FrameGraphResourceAccess::RenderTarget>(accessInfo.accessType))
    {
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = 1;
    }
    else
    {
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = 1;
    }

    imageViewInfo.subresourceRange = subresourceRange;

    imageViewInfo.setImage(image);
    return backendDevice->createImageView(imageViewInfo);
}

void FrameCoordinator::createBackingImage(FrameGraphTextureDescription* textureDesc, bool used_in_flight)
{
    if (used_in_flight)
    {
        for (auto frame : inFlightframes)
        {
            auto vmaImage = createVMAImage(textureDesc);
            auto imageName = "Frame" + std::to_string(frame->frameIdx) + "::" + textureDesc->name;
            backendDevice->setObjectDebugName(vk::Image(vmaImage.image), imageName);
            allBackingImages.emplace(imageName, vmaImage);
            auto trackedImage = std::make_unique<AccessTrackedImage>(vmaImage.image, vk::AccessFlagBits2::eNone, vk::PipelineStageFlagBits2::eNone, vk::ImageLayout::eUndefined);
            frame->backingImages.emplace(textureDesc->name,trackedImage.get());
            allBackingTrackedImages.emplace(imageName, std::move(trackedImage));
        }
    }
    else {
        auto vmaImage = createVMAImage(textureDesc);
        backendDevice->setObjectDebugName(vk::Image(vmaImage.image), textureDesc->name);
        auto trackedImage = std::make_unique<AccessTrackedImage>(vmaImage.image, vk::AccessFlagBits2::eNone, vk::PipelineStageFlagBits2::eNone, vk::ImageLayout::eUndefined);
        for (auto frame : inFlightframes)
        {
            allBackingImages.emplace(textureDesc->name, vmaImage);
            frame->backingImages.emplace(textureDesc->name, trackedImage.get());
        }
        allBackingTrackedImages.emplace(textureDesc->name, std::move(trackedImage));
    }
}

void FrameCoordinator::createBackingImageView(FrameGraphTextureDescription* textureDesc, const FrameGraphResourceAccessInfo& accessInfo, bool used_in_flight)
{
    if (used_in_flight)
    {
        auto it = allBackingImages.find("Frame0::"+ textureDesc->name);
        if (it == allBackingImages.end())
        {
            createBackingImage(textureDesc, true);
        }
        for (auto frame : inFlightframes)
        {
            auto it2 = frame->backingImages.find(textureDesc->name);
            auto imageView = createVKImageView(textureDesc, accessInfo, it2->second->image);
            allBackingImageViews.emplace("Frame" + std::to_string(frame->frameIdx) + "::" + accessInfo.pass->_name + "::" + textureDesc->name, imageView);
            frame->backingImageViews.emplace(accessInfo.pass->_name + "::" + textureDesc->name, imageView);
        }
    }
    else {
        auto it = allBackingImages.find(textureDesc->name);
        vk::Image img;
        if (it != allBackingImages.end())
        {
            img = it->second.image;
        }
        else {
            createBackingImage(textureDesc, false);
            img = allBackingImages.find(textureDesc->name)->second.image;
        }

        auto imageView = createVKImageView(textureDesc,accessInfo,img);
        allBackingImageViews.emplace(accessInfo.pass->_name + "::" + textureDesc->name, imageView);
        for (auto frame : inFlightframes)
        {
            frame->backingImageViews.emplace(accessInfo.pass->_name + "::" + textureDesc->name, imageView);
        }
    }
}

vk::DescriptorSetLayout FrameCoordinator::manageSharedDescriptorSetAOT(std::string&& name, const std::vector<vk::DescriptorSetLayoutBinding>& bindings)
{
    //Check existing descriptorSet layout
    for (auto& layoutRecord : managedDescriptorSetLayouts)
    {
        if (layoutRecord.first == bindings)
        {
            layoutRecord.second.emplace_back(name, vk::DescriptorSet{});
            return layoutRecord.first._layout;
        }
    }

    // Need to create new descriptorSetLayout Object
    vk::DescriptorSetLayoutCreateInfo layoutCreateInfo{};
    layoutCreateInfo.setBindings(bindings);
    DescriptorSetLayoutExtended newSetLayout(backendDevice->device, layoutCreateInfo);
    managedDescriptorSetLayouts.emplace_back(newSetLayout, std::vector<DescriptorSetRecord>{});
    managedDescriptorSetLayouts.back().second.emplace_back(name, vk::DescriptorSet{});
    return newSetLayout._layout;
}

vk::DescriptorSetLayout FrameCoordinator::manageSharedDescriptorSetAOT(std::string&& name, std::vector<std::tuple<vk::DescriptorType, uint32_t, vk::ShaderStageFlags>>&& bindings)
{
    std::vector<vk::DescriptorSetLayoutBinding> bindings2;
    bindings2.reserve(bindings.size());
    for (int i = 0; i < bindings.size(); i++)
    {
        vk::DescriptorSetLayoutBinding layoutBinding;
        layoutBinding.setBinding(i);
        layoutBinding.setDescriptorType(std::get<0>(bindings[i]));
        layoutBinding.setDescriptorCount(std::get<1>(bindings[i]));
        layoutBinding.setStageFlags(std::get<2>(bindings[i]));
        bindings2.emplace_back(layoutBinding);
    }

    return manageSharedDescriptorSetAOT(std::move(name), bindings2);
}

vk::DescriptorSetLayout FrameCoordinator::manageInFlightDescriptorSetAOT(std::string&& name, const std::vector<vk::DescriptorSetLayoutBinding>& bindings) {
    //Check existing descriptorSet layout
    for (auto& layoutRecord : managedDescriptorSetLayouts)
    {
        if (layoutRecord.first == bindings)
        {
            for (const auto& frame : inFlightframes)
            {
                layoutRecord.second.emplace_back("Frame"+ std::to_string(frame->frameIdx) + "::" + name, vk::DescriptorSet{});
            }
            return layoutRecord.first._layout;
        }
    }

    // Need to create new descriptorSetLayout Object
    vk::DescriptorSetLayoutCreateInfo layoutCreateInfo{};
    layoutCreateInfo.setBindings(bindings);
    DescriptorSetLayoutExtended newSetLayout(backendDevice->device, layoutCreateInfo);
    managedDescriptorSetLayouts.emplace_back(newSetLayout, std::vector<DescriptorSetRecord>{});
    for (const auto& frame : inFlightframes)
    {
        managedDescriptorSetLayouts.back().second.emplace_back("Frame" + std::to_string(frame->frameIdx) + "::" + name, vk::DescriptorSet{});
    }
    return newSetLayout._layout;
}

vk::DescriptorSetLayout FrameCoordinator::manageInFlightDescriptorSetAOT(std::string&& name, std::vector<std::tuple<vk::DescriptorType, uint32_t, vk::ShaderStageFlags>>&& bindings) {
    std::vector<vk::DescriptorSetLayoutBinding> bindings2;
    bindings2.reserve(bindings.size());
    for (int i = 0; i < bindings.size(); i++)
    {
        vk::DescriptorSetLayoutBinding layoutBinding;
        layoutBinding.setBinding(i);
        layoutBinding.setDescriptorType(std::get<0>(bindings[i]));
        layoutBinding.setDescriptorCount(std::get<1>(bindings[i]));
        layoutBinding.setStageFlags(std::get<2>(bindings[i]));
        bindings2.emplace_back(layoutBinding);
    }

    return manageInFlightDescriptorSetAOT(std::move(name), bindings2);
}

DescriptorSetRecord FrameCoordinator::findManagedDescriptorSetRecord(const std::string& name)
{
    for (const auto& layoutRecord : managedDescriptorSetLayouts)
    {
        for (const auto& descriptorSetRecord : layoutRecord.second)
        {
            if (descriptorSetRecord.first == name)
            {
                return descriptorSetRecord;
            }
        }
    }

    throw std::runtime_error("DescriptorSet" + name + "hasn't been managed yet");
}

void FrameCoordinator::updateDescriptorSetAOT(std::string&& name, VulkanWriteDescriptorSet write)
{
    auto record = findManagedDescriptorSetRecord(name);
    if (record.second != nullptr)
    {
        auto write_resolved = write;
        write_resolved.dstSet = record.second;
        pendingWriteDescriptorSets.emplace_back(write_resolved);
    }
    else {
        pendingAllocateWriteDescriptorSets.emplace_back(name, write);
    }
}

void FrameCoordinator::updateSharedDescriptorSetAOT(std::string&& name, VulkanWriteDescriptorSet write)
{
    updateDescriptorSetAOT(std::move(name),write);
}

void FrameCoordinator::updateSharedDescriptorSetAOT(std::string&& name, const std::vector<VulkanWriteDescriptorSet>& writes)
{
    for (auto & write : writes)
    {
        updateSharedDescriptorSetAOT(std::move(name),write);
    }
}

void FrameCoordinator::updateSharedDescriptorSetAOT(std::string&& name, uint32_t dstBinding, DescriptorResourceProvider provider)
{
    VulkanWriteDescriptorSet write;
    write.descriptorCount = 1;
    write.dstBinding = dstBinding;

    auto resource = provider();

    std::visit(overloaded{
        [](auto arg) { std::cout << arg << ' '; },
        [&](const vk::ImageView& image) {
            
        },
        [&](const vk::Buffer& buffer) {
            write.descriptorType = vk::DescriptorType::eUniformBuffer;
            vk::DescriptorBufferInfo bufferInfo{};
            bufferInfo.setOffset(0);
            bufferInfo.setRange(vk::WholeSize);
            bufferInfo.setBuffer(buffer);
            write.resourceInfo = bufferInfo;
        },
        [&](const vk::AccelerationStructureKHR& accel) {

        }
        }, resource);

    updateSharedDescriptorSetAOT(std::move(name), write);
}

void FrameCoordinator::updateInFlightDescriptorSetAOT(std::string&& name, std::function<VulkanWriteDescriptorSet(GPUFrame*)> writeProvider)
{
    for (auto frame : inFlightframes)
    {
        updateDescriptorSetAOT("Frame" + std::to_string(frame->frameIdx) + "::" + name, writeProvider(frame));
    }
}

void FrameCoordinator::updateInFlightDescriptorSetAOT(std::string&& name, std::function<std::vector<VulkanWriteDescriptorSet>(GPUFrame*)> writesProvider)
{
    for (auto frame : inFlightframes)
    {
        auto writeSets = writesProvider(frame);
        for (auto& write : writeSets)
        {
            updateDescriptorSetAOT("Frame" + std::to_string(frame->frameIdx) + "::" + name, write);
        }
    }
}

void FrameCoordinator::updateInFlightDescriptorSetAOT(std::string&& name, uint32_t dstBinding, vk::DescriptorType descriptorType, std::function<DescriptorResource(GPUFrame*)> provider)
{
    for (auto frame : inFlightframes)
    {
        VulkanWriteDescriptorSet write;
        write.dstBinding = dstBinding;
        write.descriptorCount = 1;
        write.descriptorType = descriptorType;

        auto resource = provider(frame);

        std::visit(overloaded{
            [](auto arg) { std::cout << arg << ' '; },
            [&](const vk::ImageView&  image) { 
                
            },
            [&](const vk::Buffer& buffer) { 
                
                vk::DescriptorBufferInfo bufferInfo{};
                bufferInfo.setOffset(0);
                bufferInfo.setRange(vk::WholeSize);
                bufferInfo.setBuffer(buffer);
                write.resourceInfo = bufferInfo;
            },
            [&](const vk::AccelerationStructureKHR& accel ){

            }
        }, resource);

        updateDescriptorSetAOT("Frame" + std::to_string(frame->frameIdx) + "::" + name, write);
    }
}

vk::DescriptorSet FrameCoordinator::getManagedDescriptorSet(std::string&& name) const
{
    return {};
}

void FrameCoordinator::prepareDescriptorSetsAOT()
{
    allocateDescriptorSetsAOT();
    doUpdateDescriptorSetsAOT();
}

void FrameCoordinator::allocateDescriptorSetsAOT()
{
    // For now we assume each descriptorSet use a dedicated pool
    managedDescriptorPools.reserve(managedDescriptorSetLayouts.size());
    std::vector<vk::DescriptorPoolSize> poolSizes{};

    for (auto& layoutRecord : managedDescriptorSetLayouts) {
        //Create per layout descriptorPool
        vk::DescriptorPoolCreateInfo poolCreateInfo{};
        poolSizes.resize(layoutRecord.first.bindingCount);
        
        uint32_t layoutDescriptorSetCount = layoutRecord.second.size();

        for (int i = 0; i < layoutRecord.first.bindingCount; i++) {
            auto& binding = layoutRecord.first.bindings[i];
            poolSizes[i].setType(binding.descriptorType);
            poolSizes[i].setDescriptorCount(binding.descriptorCount * layoutDescriptorSetCount);
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

        for (int i = 0; i < layoutDescriptorSetCount; i++)
        {
            layouts.push_back(layoutRecord.first._layout);
        }

        allocateInfo.setSetLayouts(layouts);
        allocateInfo.setDescriptorSetCount(layouts.size());
        auto allocatedResult = backendDevice->allocateDescriptorSets(allocateInfo);

        for (int i = 0; i < layoutDescriptorSetCount; i++)
        {
            auto& setRecord = layoutRecord.second[i];
            setRecord.second = allocatedResult[i];
            backendDevice->setObjectDebugName(setRecord.second, setRecord.first);

            auto frame_idx_pos = setRecord.first.find_first_of("::");
            if (frame_idx_pos != std::string::npos)
            {
                //todo : here we assume frame idx only take up one char
                uint32_t frame_idx = setRecord.first[frame_idx_pos-1] - '0';
                inFlightframes[frame_idx]->descriptorSets.emplace(setRecord.first.substr(frame_idx_pos + 2), setRecord.second);
            }
            else {
                for (auto frame : inFlightframes)
                {
                    frame->descriptorSets.emplace(setRecord.first, setRecord.second);
                }
            }
        }
    }
}

void FrameCoordinator::doUpdateDescriptorSetsAOT()
{
    for (const auto& pendingAllocateRecord : pendingAllocateWriteDescriptorSets)
    {
        auto record = findManagedDescriptorSetRecord(pendingAllocateRecord.first);
        if (record.second == nullptr)
        {
            throw std::runtime_error("DescriptorSet " + record.first + "hasn't been allocated.");
        }
        auto write = pendingAllocateRecord.second;
        write.dstSet = record.second;
        pendingWriteDescriptorSets.push_back(write);
    }
    pendingAllocateWriteDescriptorSets.clear();

    std::vector<vk::WriteDescriptorSet> writes;

    for (int i = 0; i < pendingWriteDescriptorSets.size(); i++)
    {
        auto & pendingWrite = pendingWriteDescriptorSets[i];
        vk::WriteDescriptorSet write{};
        write.setDstSet(pendingWrite.dstSet);
        write.setDstBinding(pendingWrite.dstBinding);
        write.setDstArrayElement(pendingWrite.dstArrayElement);
        write.setDescriptorCount(pendingWrite.descriptorCount);
        write.setDescriptorType(pendingWrite.descriptorType);
        if (std::holds_alternative<vk::DescriptorBufferInfo>(pendingWrite.resourceInfo))
        {
            write.setBufferInfo(std::get<vk::DescriptorBufferInfo>(pendingWrite.resourceInfo));
        }
        if (std::holds_alternative<vk::DescriptorImageInfo>(pendingWrite.resourceInfo))
        {
            write.setImageInfo(std::get<vk::DescriptorImageInfo>(pendingWrite.resourceInfo));
        }
        if (std::holds_alternative<vk::WriteDescriptorSetAccelerationStructureKHR>(pendingWrite.resourceInfo))
        {

        }
        writes.push_back(write);
    }

    backendDevice->updateDescriptorSets(writes, {});
    pendingWriteDescriptorSets.clear();
}

void FrameCoordinator::compileFrameGraphAOT()
{
    frameGraph->compileAOT(this);
}

void FrameCoordinator::update(FrameCoordinator::Event event) {
    if (event == Event::SWAPCHAIN_RESIZE)
    {
        // Currently we assume that before swapChain recreated all the frames have finished.
        std::vector<FrameGraphTexture*> outOfDatedTextures;
        outOfDatedTextures.reserve(frameGraph->textureDescriptions.size());
        for (auto& texture : frameGraph->textureDescriptions)
        {
            if (std::holds_alternative<FrameGraphTextureExtent::SwapchainRelative>(texture->extent))
            {
                outOfDatedTextures.push_back(texture.get());
            }
        }

        for (auto texture : outOfDatedTextures)
        {
            //recreate images and imageViews
            bool used_in_flight = !texture->is_persistent && texture->allow_process_in_flight;
            if (used_in_flight)
            {
                for (auto frame : inFlightframes)
                {
                    auto newImage = createVMAImage(texture);
                    auto imageName = "Frame" + std::to_string(frame->frameIdx) + "::" + texture->name;
                    backendDevice->setObjectDebugName(vk::Image(newImage.image), imageName);
                    auto oldImage = allBackingImages[imageName];
                    allBackingImages[imageName] = newImage;
                    backendDevice->deAllocateImage(oldImage.image, oldImage.allocation);

                    allBackingTrackedImages[imageName]->image = newImage.image;
                    allBackingTrackedImages[imageName]->lastAccess = vk::AccessFlagBits2::eNone;
                    allBackingTrackedImages[imageName]->lastStage = vk::PipelineStageFlagBits2::eNone;
                    allBackingTrackedImages[imageName]->lastLayout = vk::ImageLayout::eUndefined;

                    for (const auto& accessInfo : texture->accessList)
                    {
                        auto imageViewName = "Frame" + std::to_string(frame->frameIdx) + "::" + accessInfo.pass->_name + "::" + texture->name;
                        auto oldImageView = allBackingImageViews[imageViewName];
                        backendDevice->destroyImageView(oldImageView);
                        auto newImageView = createVKImageView(texture, accessInfo, newImage.image);
                        backendDevice->setObjectDebugName(newImageView, imageViewName);
                        allBackingImageViews[imageViewName] = newImageView;
                        frame->backingImageViews[accessInfo.pass->_name + "::" + texture->name] = newImageView;
                    }
                }
            }
            else {
                auto newImage = createVMAImage(texture);
                backendDevice->setObjectDebugName(vk::Image(newImage.image), texture->name);
                auto oldImage = allBackingImages[texture->name];
                allBackingImages[texture->name] = newImage;
                backendDevice->deAllocateImage(oldImage.image, oldImage.allocation);

                allBackingTrackedImages[texture->name]->image = newImage.image;
                allBackingTrackedImages[texture->name]->lastAccess = vk::AccessFlagBits2::eNone;
                allBackingTrackedImages[texture->name]->lastStage = vk::PipelineStageFlagBits2::eNone;
                allBackingTrackedImages[texture->name]->lastLayout = vk::ImageLayout::eUndefined;

                for (const auto& accessInfo : texture->accessList)
                {
                    auto imageViewName = accessInfo.pass->_name + "::" + texture->name;
                    auto oldImageView = allBackingImageViews[imageViewName];
                    backendDevice->destroyImageView(oldImageView);
                    auto newImageView = createVKImageView(texture, accessInfo, newImage.image);
                    backendDevice->setObjectDebugName(newImageView, imageViewName);
                    allBackingImageViews[imageViewName] = newImageView;
                    for (auto frame : inFlightframes)
                    {
                        frame->backingImageViews[accessInfo.pass->_name + "::" + texture->name] = newImageView;
                    }
                }
            }
        }

        for (auto frame : inFlightframes)
        {
            frame->createPresentImage();
        }

        //recreate framebuffers
        std::vector<GPURasterizedPass*> needRebuildFrameBuffers;
        for (auto texture : outOfDatedTextures)
        {
            for (const auto& accessInfo : texture->accessList)
            {
                if (std::holds_alternative<FrameGraphResourceAccess::RenderTarget>(accessInfo.accessType))
                {
                    needRebuildFrameBuffers.push_back(static_cast<GPURasterizedPass*>(accessInfo.pass));
                }
            }
        }
        for (const auto& accessInfo : frameGraph->presentTextureDesc->accessList)
        {
            if (std::holds_alternative<FrameGraphResourceAccess::RenderTarget>(accessInfo.accessType))
            {
                needRebuildFrameBuffers.push_back(static_cast<GPURasterizedPass*>(accessInfo.pass));
            }
        }
       
        // Remove duplicate elements
        std::sort(needRebuildFrameBuffers.begin(), needRebuildFrameBuffers.end());
        auto last = std::unique(needRebuildFrameBuffers.begin(), needRebuildFrameBuffers.end());
        needRebuildFrameBuffers.erase(last, needRebuildFrameBuffers.end());

        for (int i = 0; i < needRebuildFrameBuffers.size(); i++)
        {
            for (auto frame : inFlightframes)
            {
                auto oldFrameBuffer = frame->getFramebufferFor(needRebuildFrameBuffers[i]);
                backendDevice->destroy(oldFrameBuffer);
                frame->setFramebufferFor(needRebuildFrameBuffers[i], needRebuildFrameBuffers[i]->buildFrameBuffer(frame));
            }
        }

        //update pass input descriptorSets
        for (auto texture : outOfDatedTextures)
        {
            for (const auto& pass : frameGraph->_allPasses)
            {
                auto write = [texture,this](GPUPass* pass) {
                    VulkanWriteDescriptorSet write{};
                    write.dstBinding = 0;
                    write.descriptorCount = 0;
                    vk::DescriptorImageInfo imageInfo{};
                    for (auto input : pass->reads)
                    {
                        if (input->name == texture->name)
                        {
                            write.descriptorCount = 1;
                            write.descriptorType = vk::DescriptorType::eStorageImage;
                            imageInfo.imageLayout = vk::ImageLayout::eGeneral;
                            write.resourceInfo = imageInfo;
                            return write;
                        }
                        write.dstBinding++;
                    }

                    for (auto input : pass->writes)
                    {
                        if (input->name == texture->name)
                        {
                            write.descriptorCount = 1;
                            write.descriptorType = vk::DescriptorType::eStorageImage;
                            imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
                            write.resourceInfo = imageInfo;
                            return write;
                        }
                        write.dstBinding++;
                    }

                    for (auto input : pass->samples)
                    {
                        if (input->name == texture->name)
                        {
                            write.descriptorCount = 1;
                            write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
                            imageInfo.sampler = samplers[0];
                            imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
                            write.resourceInfo = imageInfo;
                            return write;
                        }
                        write.dstBinding++;
                    }

                    return write;
                }(pass.get());

                if (write.descriptorCount != 0)
                {
                    if (!texture->is_persistent && texture->allow_process_in_flight)
                    {
                        updateInFlightDescriptorSetAOT(pass->_name + "InputDescriptorSet", [&](GPUFrame* frame) {
                            std::get<vk::DescriptorImageInfo>(write.resourceInfo).imageView = frame->getBackingImageView(pass->_name + "::" + texture->name);
                            return write;
                        });
                    }
                    else {
                        std::get<vk::DescriptorImageInfo>(write.resourceInfo).imageView = getBackingImageView(pass->_name + "::" + texture->name);
                        updateSharedDescriptorSetAOT(pass->_name + "InputDescriptorSet", write);
                    }
                }
            }
        }

        doUpdateDescriptorSetsAOT();
    }
}


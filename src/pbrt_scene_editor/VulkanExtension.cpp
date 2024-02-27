//
// Created by 王泽远 on 2024/1/21.
//

#include "VulkanExtension.h"
#include "ShaderManager.h"

void SwapchainExtended::registerRecreateCallback(std::function<void(SwapchainExtended *)> callback) {
    recreateCallbacks.push_back(callback);
}

void SwapchainExtended::recreate(const vkb::Device& device)
{
    vkDeviceWaitIdle(device);

    vkb::SwapchainBuilder builder{ device };
    auto swapChain = builder
            .set_old_swapchain(this->swapchain)
            .set_desired_min_image_count(3)
            .build();

    if (!swapChain)
        throw std::runtime_error("Failed to create swapchain. Reason: " + swapChain.error().message());

    vkDestroySwapchainKHR(device, this->swapchain, VK_NULL_HANDLE);//destroy old swapchain

    static_cast<vkb::Swapchain&>(*this) = swapChain.value();

    for (auto& callback : recreateCallbacks)
    {
        callback(this);
    }

    shouldRecreate = false;
}

std::optional<VMABuffer> DeviceExtended::allocateBuffer(vk::DeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage vmaUsage) const {
    VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.size = size;
    bufferInfo.usage = usage;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = vmaUsage;

    VkBuffer buffer;
    VmaAllocation allocation;
    if(vmaCreateBuffer(_globalVMAAllocator,
                       &bufferInfo, &allocInfo,
                       &buffer, &allocation, nullptr) == VK_SUCCESS)
    {
        return VMABuffer{buffer,allocation};
    }
    return {};
}

vk::CommandBuffer DeviceExtended::allocateOnceGraphicsCommand() {
    if (onceGraphicsCommandPool == VK_NULL_HANDLE)
    {
        vk::CommandPoolCreateInfo creatInfo{};
        creatInfo.setQueueFamilyIndex(this->get_queue_index(vkb::QueueType::graphics).value());
        creatInfo.setFlags(vk::CommandPoolCreateFlagBits::eTransient);
        onceGraphicsCommandPool = this->createCommandPool(creatInfo);
    }
    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.setCommandPool(onceGraphicsCommandPool);
    allocInfo.setCommandBufferCount(1);
    allocInfo.setLevel(vk::CommandBufferLevel::ePrimary);

    vk::CommandBuffer cmdbuf{};
    auto res = this->allocateCommandBuffers(&allocInfo, &cmdbuf);
    return cmdbuf;
}

vk::CommandBuffer DeviceExtended::allocateOnceTransferCommand()
{
    if (onceTransferCommandPool == VK_NULL_HANDLE)
    {
        vk::CommandPoolCreateInfo creatInfo{};

        if(this->physical_device.has_dedicated_transfer_queue())
        {
            creatInfo.setQueueFamilyIndex(this->get_dedicated_queue_index(vkb::QueueType::transfer).value());
        }else{
            //doesn't have dedicated transfer queue. Just use main queue
            creatInfo.setQueueFamilyIndex(this->get_queue_index(vkb::QueueType::graphics).value());
        }

        creatInfo.setFlags(vk::CommandPoolCreateFlagBits::eTransient);
        onceTransferCommandPool = this->createCommandPool(creatInfo);
    }
    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.setCommandPool(onceTransferCommandPool);
    allocInfo.setCommandBufferCount(1);
    allocInfo.setLevel(vk::CommandBufferLevel::ePrimary);

    vk::CommandBuffer cmdbuf{};
    auto res = this->allocateCommandBuffers(&allocInfo, &cmdbuf);
    return cmdbuf;
}

DeviceExtended::DeviceExtended(vkb::Device device, vk::Instance instance): vkb::Device(device), vk::Device(device.device) {
    _instance = instance;
    dld = vk::DispatchLoaderDynamic(_instance, vkGetInstanceProcAddr);
    VmaVulkanFunctions vulkanFunctions = {};
    vulkanFunctions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocatorCreateInfo = {};
    allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    allocatorCreateInfo.physicalDevice = device.physical_device;
    allocatorCreateInfo.device = device;
    allocatorCreateInfo.instance = instance;
    allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;

    vmaCreateAllocator(&allocatorCreateInfo, &_globalVMAAllocator);
}

std::optional<VMAImage> DeviceExtended::allocateVMAImage(VkImageCreateInfo imageInfo) {
    VMAImage image{};
    VmaAllocationCreateInfo allocCreateInfo = {};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    if(vmaCreateImage(_globalVMAAllocator,&imageInfo,&allocCreateInfo,
                      &image.image,
                      &image.allocation,
                      &image.allocationInfo) == VK_SUCCESS)
    {
        return image;
    }

    return {};
}

std::optional<VMAImage>
DeviceExtended::allocateVMAImageForColorAttachment(VkFormat format, uint32_t width, uint32_t height,
                                                   bool sampled_need) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = format;
    imageInfo.extent = VkExtent3D{width,height,1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = sampled_need? (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |VK_IMAGE_USAGE_SAMPLED_BIT ): VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    return allocateVMAImageForAttachment(imageInfo);
}

std::optional<VMAImage>
DeviceExtended::allocateVMAImageForDepthStencilAttachment(VkFormat format, uint32_t width, uint32_t height,
                                                          bool sampled_need)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = format;
    imageInfo.extent = VkExtent3D{width,height,1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = sampled_need? (VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT):
                      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    return allocateVMAImageForAttachment(imageInfo);
}

std::optional<VMAImage> DeviceExtended::allocateVMAImageForAttachment(VkImageCreateInfo imageInfo)
{
    VMAImage image{};
    VmaAllocationCreateInfo allocCreateInfo = {};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    if(vmaCreateImage(_globalVMAAllocator,&imageInfo,&allocCreateInfo,
                      &image.image,
                      &image.allocation,
                      &image.allocationInfo) == VK_SUCCESS)
    {
        return image;
    }

    return {};
}

void DeviceExtended::oneTimeUploadSync(void* data, int size,VkBuffer dst)
{
    if (size == 0) return;

    auto blockNeededNum = (size + stagingBlockSize - 1) / stagingBlockSize;

    if (stagingBlocks.size()< blockNeededNum)
    {
        auto newBlocksNum = blockNeededNum - stagingBlocks.size();
        for (int i = 0; i < newBlocksNum; i++)
        {
            VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
            bufferCreateInfo.size = stagingBlockSize;
            bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

            VmaAllocationCreateInfo allocCreateInfo = {};
            allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

            StagingBufferBlock newBlock;
            auto result = vmaCreateBuffer(_globalVMAAllocator, &bufferCreateInfo, &allocCreateInfo,
                &newBlock.buffer,
                &newBlock.allocation,
                &newBlock.allocationInfo);

            assert(result == VK_SUCCESS);
            VkMemoryPropertyFlags memPropFlags;
            vmaGetAllocationMemoryProperties(_globalVMAAllocator, newBlock.allocation, &memPropFlags);
            assert(memPropFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
            stagingBlocks.emplace_back(newBlock);
        }
    }


    auto transfer_cmd = this->allocateOnceTransferCommand();
    vk::CommandBufferBeginInfo beginInfo{};
    transfer_cmd.begin(beginInfo);
    for (int i = 0; i < blockNeededNum; i++)
    {
        auto copySize = std::min(size - i * stagingBlockSize, stagingBlockSize);
        memcpy(stagingBlocks[i].allocationInfo.pMappedData, (char*)data + i * stagingBlockSize, copySize);
        vk::BufferCopy copyRegion{};
        copyRegion.size = copySize;
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = i * stagingBlockSize;
        transfer_cmd.copyBuffer(stagingBlocks[i].buffer, dst, copyRegion);
    }
    transfer_cmd.end();
    auto wait = submitOnceTransferCommand(transfer_cmd);
    wait();
}

void DeviceExtended::oneTimeUploadSync(void *data, uint32_t width, uint32_t height, uint32_t channel,VkImage dst, VkImageLayout layout) {
    auto size = width * height * channel;
    if (size == 0) return;

    if(imageStagingBuffer.buffer == VK_NULL_HANDLE)
    {
        VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        bufferCreateInfo.size = imageStagingBufferSize;
        bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        VmaAllocationCreateInfo allocCreateInfo = {};
        allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        auto result = vmaCreateBuffer(_globalVMAAllocator, &bufferCreateInfo, &allocCreateInfo,
                                      &imageStagingBuffer.buffer,
                                      &imageStagingBuffer.allocation,
                                      &imageStagingBuffer.allocationInfo);

        assert(result == VK_SUCCESS);
        VkMemoryPropertyFlags memPropFlags;
        vmaGetAllocationMemoryProperties(_globalVMAAllocator, imageStagingBuffer.allocation, &memPropFlags);
        assert(memPropFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    }

    /*
     * Different from copying between buffers, it's trickier to copy multiple buffer blocks into one image.
     * So we just enlarge the buffer once it's no longer able to contain the whole data
     */

    if(size > imageStagingBufferSize)
    {
        deAllocateBuffer(imageStagingBuffer.buffer,imageStagingBuffer.allocation);

        VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        bufferCreateInfo.size = size;
        bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        VmaAllocationCreateInfo allocCreateInfo = {};
        allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        auto result = vmaCreateBuffer(_globalVMAAllocator, &bufferCreateInfo, &allocCreateInfo,
                                      &imageStagingBuffer.buffer,
                                      &imageStagingBuffer.allocation,
                                      &imageStagingBuffer.allocationInfo);

        assert(result == VK_SUCCESS);
        VkMemoryPropertyFlags memPropFlags;
        vmaGetAllocationMemoryProperties(_globalVMAAllocator, imageStagingBuffer.allocation, &memPropFlags);
        assert(memPropFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        imageStagingBufferSize = size;
    }

    vk::ImageSubresourceRange subresourceRange;
    subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor);
    subresourceRange.setBaseMipLevel(0);
    subresourceRange.setLevelCount(1);
    subresourceRange.setBaseArrayLayer(0);
    subresourceRange.setLayerCount(1);

    auto transfer_cmd = this->allocateOnceTransferCommand();
    vk::CommandBufferBeginInfo beginInfo{};
    transfer_cmd.begin(beginInfo);
    vk::ImageMemoryBarrier barrier;
    barrier.setSrcAccessMask(vk::AccessFlagBits::eNone);
    barrier.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
    barrier.setOldLayout(vk::ImageLayout::eUndefined);
    barrier.setNewLayout(vk::ImageLayout::eTransferDstOptimal);
    barrier.setSrcQueueFamilyIndex(vk::QueueFamilyIgnored);
    barrier.setDstQueueFamilyIndex(vk::QueueFamilyIgnored);
    barrier.setImage(dst);
    barrier.setSubresourceRange(subresourceRange);
    transfer_cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,vk::PipelineStageFlagBits::eTransfer,{},0,{},barrier);

    vk::ImageSubresourceLayers subresourceLayers;
    subresourceLayers.setAspectMask(subresourceRange.aspectMask);
    subresourceLayers.setMipLevel(0);
    subresourceLayers.setLayerCount(1);
    subresourceLayers.setBaseArrayLayer(0);
    memcpy(imageStagingBuffer.allocationInfo.pMappedData, data, size);
    vk::BufferImageCopy region{};
    region.setImageSubresource(subresourceLayers);
    region.setBufferImageHeight(0);
    region.setBufferOffset(0);
    region.setBufferRowLength(0);
    region.setImageOffset({0,0,0});
    region.setImageExtent({width,height,1});

    transfer_cmd.copyBufferToImage(imageStagingBuffer.buffer,dst,vk::ImageLayout::eTransferDstOptimal,region);

    vk::ImageMemoryBarrier barrier2;
    barrier2.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
    barrier2.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
    barrier2.setOldLayout(vk::ImageLayout::eTransferDstOptimal);
    barrier2.setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
    barrier2.setSrcQueueFamilyIndex(vk::QueueFamilyIgnored);
    barrier2.setDstQueueFamilyIndex(vk::QueueFamilyIgnored);
    barrier2.setImage(dst);
    barrier2.setSubresourceRange(subresourceRange);
    transfer_cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,vk::PipelineStageFlagBits::eVertexShader,{},0,{},barrier2);
    transfer_cmd.end();
    auto wait = submitOnceTransferCommand(transfer_cmd);
    wait();
}

bool VulkanGraphicsPipeline::compatibleWithVertexShader(const std::string &shaderVariantUUID) {
    return _vs->_uuid == shaderVariantUUID;
}

bool VulkanGraphicsPipeline::compatibleWithFragmentShader(const std::string &shaderVariantUUID) {
    return _fs->_uuid == shaderVariantUUID;
}

VulkanGraphicsPipelineBuilder::VulkanGraphicsPipelineBuilder(vk::Device device,
                                                             VertexShader* vs,
                                                             FragmentShader* fs,
                                                             vk::PipelineVertexInputStateCreateInfo vertexInputInfo,
                                                             vk::RenderPass renderPass) {
    _device = device;
    _vs = vs;
    _fs = fs;
    _vertexInputInfo = VulkanPipelineVertexInputStateInfo(vertexInputInfo);
    _renderPass = renderPass;

    //set default state
    _InputAssemblyStateInfo.setTopology(vk::PrimitiveTopology::eTriangleList);
    _InputAssemblyStateInfo.setPrimitiveRestartEnable(vk::False);

    _RasterizationStateInfo.setRasterizerDiscardEnable(vk::False);
    _RasterizationStateInfo.setFrontFace(vk::FrontFace::eClockwise);
    _RasterizationStateInfo.setPolygonMode(vk::PolygonMode::eFill);
    _RasterizationStateInfo.setCullMode(vk::CullModeFlagBits::eBack);
    _RasterizationStateInfo.setLineWidth(1.0);

    auto colorComponentAll = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG|vk::ColorComponentFlagBits::eB|vk::ColorComponentFlagBits::eA;
    defaultAttachmentState.setColorWriteMask(colorComponentAll);
    defaultAttachmentState.setSrcColorBlendFactor(vk::BlendFactor::eOne);
    defaultAttachmentState.setDstColorBlendFactor(vk::BlendFactor::eZero);
    defaultAttachmentState.setColorBlendOp(vk::BlendOp::eAdd);
    defaultAttachmentState.setSrcAlphaBlendFactor(vk::BlendFactor::eOne);
    defaultAttachmentState.setDstAlphaBlendFactor(vk::BlendFactor::eZero);
    defaultAttachmentState.setAlphaBlendOp(vk::BlendOp::eAdd);
    defaultAttachmentState.blendEnable = VK_FALSE;

    _ColorBlendStateInfo.setAttachments(defaultAttachmentState);
    _ColorBlendStateInfo.setBlendConstants({1,1,1,1});
    _ColorBlendStateInfo.setLogicOpEnable(vk::False);

    _MultisampleStateInfo.setRasterizationSamples(vk::SampleCountFlagBits::e1);
    _MultisampleStateInfo.setSampleShadingEnable(vk::False);

    _ViewportStateInfo.setViewportCount(1);
    _ViewportStateInfo.setScissorCount(1);

    // By default, use dynamic viewport for flexible
    _DynamicStateInfo.setDynamicStates(dynamicStates);
}

VulkanGraphicsPipeline VulkanGraphicsPipelineBuilder::build() const {
    assert(_device!=VK_NULL_HANDLE);
    assert(_renderPass!=VK_NULL_HANDLE);

    vk::GraphicsPipelineCreateInfo createInfo{};
    std::array<vk::PipelineShaderStageCreateInfo,2> stages{_vs->getStageCreateInfo(),_fs->getStageCreateInfo()};
    createInfo.setStages(stages);
    createInfo.setRenderPass(_renderPass);
    createInfo.setLayout(_pipelineLayout);
    auto vis = _vertexInputInfo.getCreateInfo();
    createInfo.setPVertexInputState(&vis);
    createInfo.setPInputAssemblyState(&_InputAssemblyStateInfo);
    createInfo.setPColorBlendState(&_ColorBlendStateInfo);
    createInfo.setPDepthStencilState(&_DepthStencilStateInfo);
    createInfo.setPRasterizationState(&_RasterizationStateInfo);
    createInfo.setPMultisampleState(&_MultisampleStateInfo);
    createInfo.setPViewportState(&_ViewportStateInfo);
    createInfo.setPDynamicState(&_DynamicStateInfo);

    auto newPipeline = _device.createGraphicsPipeline(VK_NULL_HANDLE,createInfo);
    if(newPipeline.result != vk::Result::eSuccess)
    {
        throw std::runtime_error("Failed to create pipeline");
    }

    VulkanGraphicsPipeline pipeline;
    // fill the metadata
    pipeline._pipeline = newPipeline.value;
    pipeline.device = _device;
    pipeline.renderPass = _renderPass;
    pipeline.pipelineLayout = _pipelineLayout;
    pipeline._vs= _vs;
    pipeline._fs = _fs;
    pipeline.vertexInputStateInfo = VulkanPipelineVertexInputStateInfo(_vertexInputInfo);
    pipeline._InputAssemblyStateInfo = _InputAssemblyStateInfo;
    pipeline._ColorBlendStateInfo = _ColorBlendStateInfo;
    pipeline._DepthStencilStateInfo = _DepthStencilStateInfo;
    pipeline._RasterizationStateInfo = _RasterizationStateInfo;
    pipeline._MultisampleStateInfo = _MultisampleStateInfo;
    pipeline._ViewportStateInfo = _ViewportStateInfo;
    pipeline._DynamicStateInfo = _DynamicStateInfo;

    return pipeline;
}
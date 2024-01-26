//
// Created by 王泽远 on 2024/1/21.
//

#include "VulkanExtension.h"

void SwapchainExtended::registerRecreateCallback(std::function<void(SwapchainExtended *)> callback) {
    recreateCallbacks.push_back(callback);
}

void SwapchainExtended::destroy(const vkb::Device &device) {
    vkDestroySwapchainKHR(device, this->swapchain,VK_NULL_HANDLE);
}

void SwapchainExtended::recreate(const vkb::Device& device)
{
    vkDeviceWaitIdle(device);

    destroy(device);//destroy old swapchain

    vkb::SwapchainBuilder builder{ device };
    auto swapChain = builder
            .set_old_swapchain(VK_NULL_HANDLE)
            .set_desired_min_image_count(3)
            .build();

    if (!swapChain)
        throw std::runtime_error("Failed to create swapchain. Reason: " + swapChain.error().message());

    static_cast<vkb::Swapchain&>(*this) = swapChain.value();

    for (auto& callback : recreateCallbacks)
    {
        callback(this);
    }
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

void DeviceExtended::oneTimeUploadSync(void* data, int size,VkBuffer dst)
{
    if(std::get<0>(stagingBuffer) == VK_NULL_HANDLE)
    {
        VkBufferCreateInfo bufferCreateInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufferCreateInfo.size = 32 * 1024 * 1024; // 32MB
        bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        VmaAllocationCreateInfo allocCreateInfo = {};
        allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        vmaCreateBuffer(_globalVMAAllocator,&bufferCreateInfo,&allocCreateInfo,
                        &std::get<0>(stagingBuffer),
                        &std::get<1>(stagingBuffer),
                        &std::get<2>(stagingBuffer));
    }

    memcpy(std::get<2>(stagingBuffer).pMappedData,data,size);

    auto transfer_cmd = this->allocateOnceTransferCommand();
    vk::CommandBufferBeginInfo beginInfo{};
    transfer_cmd.begin(beginInfo);
    vk::BufferCopy copyRegion{};
    copyRegion.size =size;
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    transfer_cmd.copyBuffer(std::get<0>(stagingBuffer),dst,copyRegion);
    transfer_cmd.end();
    auto wait_handle = this->submitOnceTransferCommand(transfer_cmd);
    wait_handle();
}
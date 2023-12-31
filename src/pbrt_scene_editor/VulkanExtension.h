#pragma once

#include <memory>
#include <functional>

#include <vulkan/vulkan.hpp>
#include "VkBootstrap.h"

struct SwapchainExtended : vkb::Swapchain
{
    SwapchainExtended(){}

    SwapchainExtended(const vkb::Swapchain& swapchain) : vkb::Swapchain(swapchain){
    }

    void registerRecreateCallback(std::function<void(SwapchainExtended*)> callback) {
        recreateCallbacks.push_back(callback);
    }
    
    void destroy(const vkb::Device& device)
    {
        vkDestroySwapchainKHR(device, this->swapchain,VK_NULL_HANDLE);
    }
    
    void recreate(const vkb::Device& device)
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

    vk::SwapchainKHR getRawVKSwapChain() const {
        return this->swapchain;
    }

private:
    std::vector<std::function<void(SwapchainExtended*)>> recreateCallbacks;
};

struct DeviceExtended : vkb::Device, vk::Device
{
    DeviceExtended() {

    }

    DeviceExtended(vkb::Device device,vk::Instance instance) : vkb::Device(device), vk::Device(device.device) {
        _instance = instance;
    }

    void setSwapchain(vkb::Swapchain swapchain) {
        _swapchain = swapchain;
    }
    
    void recreateSwapchain()
    {
        _swapchain.recreate(*this);
    }

    vk::CommandBuffer allocateOnceGraphicsCommand()
    {
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

        vk::CommandBuffer cmdbuf;
        this->allocateCommandBuffers(&allocInfo, &cmdbuf);
        return cmdbuf;
    }

    auto submitOnceGraphicsCommand(vk::CommandBuffer cmd)
    {
        vk::SubmitInfo submitInfo{};
        submitInfo.setCommandBufferCount(1);
        submitInfo.setCommandBuffers(cmd);
        vk::Queue queue = this->get_queue(vkb::QueueType::graphics).value();
        queue.submit(submitInfo);

        return [=]{
            queue.waitIdle();
            this->freeCommandBuffers(onceGraphicsCommandPool,cmd);
        };
    }

    //vkb::Device _vkb_device;
    //vk::Device _vk_device;
    vk::Instance _instance;
    SwapchainExtended _swapchain;
private:
    vk::CommandPool onceGraphicsCommandPool = VK_NULL_HANDLE;
};

struct CommandPoolExtended : vk::CommandPool
{
    CommandPoolExtended() {}
    CommandPoolExtended(std::shared_ptr<DeviceExtended> device, vk::CommandPool pool) :_device(device), _cmdPool(pool){}

    vk::CommandPool _cmdPool;
    std::shared_ptr<DeviceExtended> _device;

    vk::CommandBuffer allocateCommandBuffer(vk::CommandBufferLevel level) const
    {
        vk::CommandBufferAllocateInfo allocInfo{};
        allocInfo.commandPool = _cmdPool;
        allocInfo.commandBufferCount = 1;
        allocInfo.level = level;

        return _device->allocateCommandBuffers(allocInfo).front();
    }

    std::vector<vk::CommandBuffer> allocateCommandBuffers(vk::CommandBufferLevel level, uint32_t count) const
    {
        vk::CommandBufferAllocateInfo allocInfo{};
        allocInfo.commandPool = _cmdPool;
        allocInfo.commandBufferCount = count;
        allocInfo.level = level;

        return _device->allocateCommandBuffers(allocInfo);
    }
};

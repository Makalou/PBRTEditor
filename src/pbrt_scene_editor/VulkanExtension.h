#pragma once

#include <memory>
#include <functional>

#include <vulkan/vulkan.hpp>
#include "VkBootstrap.h"
#include "vk_mem_alloc.h"
#include "VMAExtension.h"

struct SwapchainExtended : vkb::Swapchain
{
    SwapchainExtended(){}

    SwapchainExtended(const vkb::Swapchain& swapchain) : vkb::Swapchain(swapchain){
    }

    void registerRecreateCallback(std::function<void(SwapchainExtended*)> callback);
    
    void destroy(const vkb::Device& device);
    
    void recreate(const vkb::Device& device);

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

    DeviceExtended(vkb::Device device,vk::Instance instance);

    std::optional<VMABuffer> allocateBuffer(vk::DeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage vmaUsage) const;

    std::optional<VMABuffer> allocateBuffer(vk::DeviceSize size, VkBufferUsageFlags usage)
    {
        return allocateBuffer(size,usage,VMA_MEMORY_USAGE_AUTO);
    }

    template<typename T>
    std::optional<VMAObservedBufferMapped<T>> allocateObservedBufferPull(VkBufferUsageFlagBits usage)
    {
        VMAObservedBufferMapped<T> mappedBuffer;
        VkBufferCreateInfo bufferCreateInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufferCreateInfo.size = sizeof(T);
        bufferCreateInfo.usage = usage;

        VmaAllocationCreateInfo allocCreateInfo = {};
        allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        if(vmaCreateBuffer(_globalVMAAllocator,&bufferCreateInfo,&allocCreateInfo,
                        &mappedBuffer.buffer,
                        &mappedBuffer.allocation,
                        &mappedBuffer.allocationInfo) == VK_SUCCESS)
        {
            VkMemoryPropertyFlags memPropFlags;
            vmaGetAllocationMemoryProperties(_globalVMAAllocator,mappedBuffer.allocation, &memPropFlags);
            assert(memPropFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
            return mappedBuffer;
        }

        return {};
    }

    template<typename T>
    std::optional<VMAObservedBufferPush<T>> allocateObservedBufferPush(VkBufferUsageFlagBits usage)
    {
        return {};
    }

    void deAllocateBuffer(VkBuffer buffer, VmaAllocation allocation)
    {
        vmaDestroyBuffer(_globalVMAAllocator,buffer,allocation);
    }

    std::optional<VMAImage> allocateVMAImage(VkImageCreateInfo imageInfo);

    /*
     * Exclusive sharing mode
     */
    std::optional<VMAImage> allocateVMAImageForColorAttachment(VkFormat format, uint32_t width, uint32_t height, bool sampled_need = false);

    /*
     * Exclusive sharing mode
     */
    std::optional<VMAImage> allocateVMAImageForDepthStencilAttachment(VkFormat format, uint32_t width, uint32_t height, bool sampled_need = false);

    std::optional<VMAImage> allocateVMAImageForAttachment(VkImageCreateInfo imageInfo);

    void deAllocateImage(VkImage image, VmaAllocation allocation)
    {
        vmaDestroyImage(_globalVMAAllocator,image,allocation);
    }

    void setSwapchain(vkb::Swapchain swapchain) {
        _swapchain = swapchain;
    }
    
    void recreateSwapchain()
    {
        _swapchain.recreate(*this);
    }

    vk::CommandBuffer allocateOnceGraphicsCommand();

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

    vk::CommandBuffer allocateOnceTransferCommand();

    auto submitOnceTransferCommand(vk::CommandBuffer cmd)
    {
        vk::SubmitInfo submitInfo{};
        submitInfo.setCommandBufferCount(1);
        submitInfo.setCommandBuffers(cmd);
        auto transferQueue = this->get_queue(vkb::QueueType::transfer);
        vk::Queue queue;
        if(transferQueue)
        {
            queue = transferQueue.value();
        }else{
            queue =  this->get_queue(vkb::QueueType::graphics).value();
        }

        queue.submit(submitInfo);

        return [=]{
            queue.waitIdle();
            this->freeCommandBuffers(onceTransferCommandPool,cmd);
        };
    }

    /*
     * Upload one-time resource (vertex(index) buffer, texture buffer), in blocking way.
     * Not thread-safe! (staging buffer is not thread-safe).
     * */
    void oneTimeUploadSync(void* data, int size,VkBuffer dst);

    auto createDescriptorSetLayout2(std::vector<vk::DescriptorSetLayoutBinding>&& descripotSetBindings)
    {
        vk::DescriptorSetLayoutCreateInfo createInfo{};
        createInfo.setBindings(descripotSetBindings);
        return createDescriptorSetLayout(createInfo);
    }

    auto createDescriptorSetLayout2(const std::vector<vk::DescriptorSetLayoutBinding>& descripotSetBindings)
    {
        vk::DescriptorSetLayoutCreateInfo createInfo{};
        createInfo.setBindings(descripotSetBindings);
        return createDescriptorSetLayout(createInfo);
    }

    auto allocateDescriptorSets2(vk::DescriptorPool pool, int count, vk::DescriptorSetLayout layout)
    {
        vk::DescriptorSetAllocateInfo allocateInfo{};
        allocateInfo.setDescriptorPool(pool);
        allocateInfo.setSetLayouts(layout);
        allocateInfo.setDescriptorSetCount(count);
        return allocateDescriptorSets(allocateInfo);
    }

    vk::DescriptorSet allocateSingleDescriptorSet(vk::DescriptorPool pool, vk::DescriptorSetLayout layout)
    {
        return allocateDescriptorSets2(pool,1,layout)[0];
    }

    auto createPipelineLayout2(std::vector<vk::DescriptorSetLayout>&& descriptorSetLayouts)
    {
        vk::PipelineLayoutCreateInfo createInfo{};
        createInfo.setSetLayouts(descriptorSetLayouts);
        return createPipelineLayout(createInfo);
    }

    ~DeviceExtended(){
        vmaDestroyAllocator(_globalVMAAllocator);
    }

    vk::Instance _instance;
    SwapchainExtended _swapchain;
    VmaAllocator _globalVMAAllocator;
    std::tuple<VkBuffer,VmaAllocation,VmaAllocationInfo> stagingBuffer{};
private:
    vk::CommandPool onceGraphicsCommandPool = VK_NULL_HANDLE;
    vk::CommandPool onceTransferCommandPool = VK_NULL_HANDLE;
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

    void reset()
    {
        //To reset the pool the flag RESET_COMMAND_BUFFER_BIT is not required,
        // and it is actually better to avoid it since it prevents it from using
        // a single large allocator for all buffers in the pool thus increasing memory overhead.
        //https://arm-software.github.io/vulkan_best_practice_for_mobile_developers/samples/performance/command_buffer_usage/command_buffer_usage_tutorial.html#resetting-the-command-pool
        vkResetCommandPool(_device->device,_cmdPool,0);
    }
};

//https://docs.vulkan.org/spec/latest/chapters/cmdbuffers.html#VkCommandBufferUsageFlagBits
struct LinearCachedCommandAllocator {
    explicit LinearCachedCommandAllocator(CommandPoolExtended pool) : _pool(pool)
    {

    }

    vk::CommandBuffer getOrAllocateNextPrimary()
    {
        if(_current_primary_idx < allocatedPrimaryCommandBuffers.size())
        {
            return allocatedPrimaryCommandBuffers[_current_primary_idx++];
        }

        auto cmdBuf = _pool.allocateCommandBuffer(vk::CommandBufferLevel::ePrimary);
        _current_primary_idx ++;
        allocatedPrimaryCommandBuffers.push_back(cmdBuf);
        return cmdBuf;
    }

    vk::CommandBuffer getOrAllocateNextSecondary()
    {
        if(_current_secondary_idx < allocatedSecondaryCommandBuffers.size())
        {
            return allocatedSecondaryCommandBuffers[_current_primary_idx++];
        }

        auto cmdBuf = _pool.allocateCommandBuffer(vk::CommandBufferLevel::eSecondary);
        _current_secondary_idx++;
        allocatedSecondaryCommandBuffers.push_back(cmdBuf);
        return cmdBuf;
    }

    /*
     * Reset a linear cached allocator will not free any command allocated.
     * It's caller's responsibility to guarantee that all the commands have finished execution.
     */
    void reset()
    {
        _current_primary_idx = 0;
        _current_secondary_idx = 0;
        _pool.reset();
    }

    CommandPoolExtended _pool;
    std::vector<vk::CommandBuffer> allocatedPrimaryCommandBuffers;
    int _current_primary_idx = 0;
    std::vector<vk::CommandBuffer> allocatedSecondaryCommandBuffers;
    int _current_secondary_idx = 0;
};

#define VK_GRAPHICS_PIPELINE_STATE_DEF_HELPER(state) vk::Pipeline##state##StateCreateInfo _##state##StateInfo{}; \
                                                        void set##state##Info(const vk::Pipeline##state##StateCreateInfo & info) { \
                                                             _##state##StateInfo = info;                                          \
                                                        }
struct VulkanGraphicsPipeline
{
    vk::Pipeline _pipeline = VK_NULL_HANDLE;
    VK_GRAPHICS_PIPELINE_STATE_DEF_HELPER(InputAssembly)
    VK_GRAPHICS_PIPELINE_STATE_DEF_HELPER(ColorBlend)
    VK_GRAPHICS_PIPELINE_STATE_DEF_HELPER(DepthStencil)
    VK_GRAPHICS_PIPELINE_STATE_DEF_HELPER(Rasterization)
    VK_GRAPHICS_PIPELINE_STATE_DEF_HELPER(Multisample)
    VK_GRAPHICS_PIPELINE_STATE_DEF_HELPER(Viewport)
    VK_GRAPHICS_PIPELINE_STATE_DEF_HELPER(Dynamic)

    vk::Device device = VK_NULL_HANDLE;
    vk::PipelineShaderStageCreateInfo vsInfo;
    vk::PipelineShaderStageCreateInfo fsInfo;
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
    vk::RenderPass renderPass = VK_NULL_HANDLE ;
    vk::PipelineLayout pipelineLayout = VK_NULL_HANDLE ;

    vk::PipelineColorBlendAttachmentState defaultAttachmentState{};

    VulkanGraphicsPipeline(vk::Device _device,
        vk::PipelineShaderStageCreateInfo _vsInfo,
        vk::PipelineShaderStageCreateInfo _fsInfo,
        vk::PipelineVertexInputStateCreateInfo _vertexInputInfo,
        vk::RenderPass _renderPass,
        vk::PipelineLayout _pipelineLayout)
    {
        device = _device;
        vsInfo = _vsInfo;
        fsInfo = _fsInfo;
        vertexInputInfo = _vertexInputInfo;
        renderPass = _renderPass;
        pipelineLayout = _pipelineLayout;

        //set default state
        _InputAssemblyStateInfo.setTopology(vk::PrimitiveTopology::eTriangleStrip);
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
        auto dynamicStates = {vk::DynamicState::eViewport,vk::DynamicState::eScissor};
        _DynamicStateInfo.setDynamicStates(dynamicStates);
    }

    void setInputAssemblyState(const vk::PipelineInputAssemblyStateCreateInfo& ){

    };

    /*
     *  Make sure you have set all desired state before calling create
     */
    void build()
    {
        assert(device!=VK_NULL_HANDLE);
        assert(renderPass!=VK_NULL_HANDLE);
        //assert(pipelineLayout!=VK_NULL_HANDLE);
        if(pipelineLayout == VK_NULL_HANDLE)
        {
            //create empty pipeline layout
            vk::PipelineLayoutCreateInfo emptyLayoutInfo{};
            emptyLayoutInfo.setSetLayoutCount(0);
            pipelineLayout = device.createPipelineLayout(emptyLayoutInfo);
        }

        vk::GraphicsPipelineCreateInfo createInfo{};
        std::array<vk::PipelineShaderStageCreateInfo,2> stages{vsInfo,fsInfo};
        createInfo.setStages(stages);
        createInfo.setRenderPass(renderPass);
        createInfo.setLayout(pipelineLayout);
        createInfo.setPVertexInputState(&vertexInputInfo);
        createInfo.setPInputAssemblyState(&_InputAssemblyStateInfo);
        createInfo.setPColorBlendState(&_ColorBlendStateInfo);
        createInfo.setPDepthStencilState(&_DepthStencilStateInfo);
        createInfo.setPRasterizationState(&_RasterizationStateInfo);
        createInfo.setPMultisampleState(&_MultisampleStateInfo);
        createInfo.setPViewportState(&_ViewportStateInfo);
        createInfo.setPDynamicState(&_DynamicStateInfo);

        auto newPipeline = device.createGraphicsPipeline(nullptr,createInfo);
        if(newPipeline.result != vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to create pipeline");
        }
        _pipeline = newPipeline.value;
    }
};


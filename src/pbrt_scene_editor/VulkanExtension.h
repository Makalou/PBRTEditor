#pragma once

#include <memory>
#include <functional>

#include <vulkan/vulkan.hpp>
#include "VkBootstrap.h"
#include "vk_mem_alloc.h"
#include "VMAExtension.h"
#include <optional>
#include <iostream>

struct SwapchainExtended : vkb::Swapchain
{
    SwapchainExtended(){}

    SwapchainExtended(const vkb::Swapchain& swapchain) : vkb::Swapchain(swapchain){
    }

    void registerRecreateCallback(std::function<void(SwapchainExtended*)> callback);
    
    void recreate(const vkb::Device& device);

    vk::SwapchainKHR getRawVKSwapChain() const {
        return this->swapchain;
    }

    vk::Viewport getDefaultViewport() const
    {
        vk::Viewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)this->extent.width;
        viewport.height = (float)this->extent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        return viewport;
    }

    vk::Rect2D getDefaultScissor() const
    {
        vk::Rect2D scissor{};
        scissor.offset = vk::Offset2D{ 0, 0 };
        scissor.extent = this->extent;
        return scissor;
    }

    bool shouldRecreate = false;

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

    template<class ObjectT>
    auto setObjectDebugName(ObjectT object,const char* name) const
    {
        vk::DebugUtilsObjectNameInfoEXT debugNameInfo{};
        debugNameInfo.setObjectType(ObjectT::objectType);
        debugNameInfo.setObjectHandle((uint64_t)static_cast<ObjectT::CType>(object));
        debugNameInfo.setPObjectName(name);
        setDebugUtilsObjectNameEXT(debugNameInfo, dld);
    }

    auto createDescriptorSetLayout2(std::vector<vk::DescriptorSetLayoutBinding>&& descripotSetBindings) const
    {
        vk::DescriptorSetLayoutCreateInfo createInfo{};
        createInfo.setBindings(descripotSetBindings);
        return createDescriptorSetLayout(createInfo);
    }

    auto createDescriptorSetLayout2(const std::vector<vk::DescriptorSetLayoutBinding>& descripotSetBindings) const
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

    void updateDescriptorSetUniformBuffer(vk::DescriptorSet dstSet,uint32_t dstBinding,vk::Buffer buffer,
                                          vk::DeviceSize range, vk::DeviceSize offset)
    {
        vk::WriteDescriptorSet write;
        write.setDstSet(dstSet);
        write.setDescriptorType(vk::DescriptorType::eUniformBuffer);
        write.setDescriptorCount(1);
        write.setDstBinding(dstBinding);

        vk::DescriptorBufferInfo bufInfo{};
        bufInfo.setBuffer(buffer);
        bufInfo.setRange(range);
        bufInfo.setOffset(offset);

        write.setBufferInfo(bufInfo);
        this->updateDescriptorSets(write,{});
    }

    void updateDescriptorSetUniformBuffer(vk::DescriptorSet dstSet,uint32_t dstBinding,vk::Buffer buffer)
    {
        updateDescriptorSetUniformBuffer(dstSet,0,buffer,vk::WholeSize,0);
    }

    auto createPipelineLayout2(std::vector<vk::DescriptorSetLayout>&& descriptorSetLayouts)
    {
        vk::PipelineLayoutCreateInfo createInfo{};
        createInfo.setSetLayouts(descriptorSetLayouts);
        return createPipelineLayout(createInfo);
    }

    auto getSupportedDepthStencilFormat() const
    {
        VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
        VkFormatFeatureFlags features = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;

        VkFormat candidates[] = {
                VK_FORMAT_D16_UNORM,
                VK_FORMAT_D16_UNORM_S8_UINT,
                VK_FORMAT_D24_UNORM_S8_UINT,
                VK_FORMAT_D32_SFLOAT,
                VK_FORMAT_D32_SFLOAT_S8_UINT
        };
        std::vector<VkFormat> formats;

        for(auto & format : candidates)
        {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(this->physical_device,format, &props);

            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
                formats.push_back(format);
            }
            else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
                formats.push_back(format);
            }
        }

        return formats;
    }

    auto getDLD()
    {
        return dld;
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
    vk::DispatchLoaderDynamic dld;
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

struct VulkanDescriptorSetLayout
{
    VulkanDescriptorSetLayout(vk::Device device, const vk::DescriptorSetLayoutCreateInfo& info)
    {
        bindingCount = info.bindingCount;
        bindings.reset(new vk::DescriptorSetLayoutBinding[bindingCount]);

        for(int i = 0; i < bindingCount; i++)
        {
            bindings[i] = info.pBindings[i];
        }

        _layout = device.createDescriptorSetLayout(info);
    }

    ~VulkanDescriptorSetLayout()
    {
        if(bindings.use_count() == 1)
        {
            device.destroy(_layout);
        }
    }

    bool operator == (const VulkanDescriptorSetLayout& other) const
    {
        if(this->bindingCount!=other.bindingCount) return false;
        //todo for now we assume the binding other matter
        for(int i = 0; i < bindingCount; i++)
        {
            if(bindings[i].binding != other.bindings[i].binding) return false;
            if(bindings[i].descriptorCount != other.bindings[i].descriptorCount) return false;
            if(bindings[i].descriptorType != other.bindings[i].descriptorType) return false;
            if(bindings[i].stageFlags != other.bindings[i].stageFlags) return false;
            //todo immutable samplers
        }
        return true;
    }

    bool operator ==(const vk::DescriptorSetLayoutCreateInfo& info) const
    {
        //todo
        return true;
    }

    vk::Device device;
    vk::DescriptorSetLayout _layout = VK_NULL_HANDLE;
    std::shared_ptr<vk::DescriptorSetLayoutBinding[]> bindings;
    int bindingCount = 0;
};

struct VulkanPipelineLayout
{
    VulkanPipelineLayout(std::vector<VulkanDescriptorSetLayout>&& setLayouts)
    {
        setLayoutsCount = setLayouts.size();
        device = setLayouts[0].device;
        pSetLayouts.reset((VulkanDescriptorSetLayout*)new char[setLayoutsCount * sizeof(VulkanDescriptorSetLayout)]);
        for(int i = 0; i < setLayoutsCount; i++)
        {
            *(pSetLayouts.get() + i) = setLayouts[i];
        }
    }

    ~VulkanPipelineLayout()
    {
        if(pSetLayouts.use_count() == 1)
        {
            device.destroy(_layout);
        }
    }

    bool operator == (const vk::PipelineLayoutCreateInfo info) const
    {
        //todo
        return true;
    }

    bool operator == (const VulkanPipelineLayout& other) const
    {
        //todo
        return true;
    }

    bool operator == (std::vector<VulkanDescriptorSetLayout>&& setLayouts) const
    {
        if(setLayouts.size() != setLayoutsCount) return false;
        for(int i = 0; i < setLayoutsCount; i++)
        {
            if(!(*(pSetLayouts.get() + i) == setLayouts[i])) return false;
        }
        return true;
    }

    vk::Device device;
    vk::PipelineLayout _layout;
    std::shared_ptr<VulkanDescriptorSetLayout> pSetLayouts;
    int setLayoutsCount = 0;
};

struct VulkanPipelineVertexInputStateInfo
{
    VulkanPipelineVertexInputStateInfo() = default;

    explicit VulkanPipelineVertexInputStateInfo(const vk::PipelineVertexInputStateCreateInfo & info)
    {
        bindingCount = info.vertexBindingDescriptionCount;
        attributeCount = info.vertexAttributeDescriptionCount;
        for(int i = 0; i < bindingCount; i ++)
        {
            bindings[i] = info.pVertexBindingDescriptions[i];
        }
        for(int i = 0; i < attributeCount; i++)
        {
            attributes[i] = info.pVertexAttributeDescriptions[i];
        }
    }

    vk::PipelineVertexInputStateCreateInfo getCreateInfo() const
    {
        vk::PipelineVertexInputStateCreateInfo createInfo{};
        createInfo.vertexBindingDescriptionCount = bindingCount;
        createInfo.vertexAttributeDescriptionCount = attributeCount;
        createInfo.pVertexBindingDescriptions = bindings;
        createInfo.pVertexAttributeDescriptions = attributes;
        return createInfo;
    }

    vk::VertexInputBindingDescription bindings[16];
    vk::VertexInputAttributeDescription attributes[16];

    int bindingCount = 0;
    int attributeCount = 0;

    bool operator==(const VulkanPipelineVertexInputStateInfo& other) const
    {
        if(bindingCount!=other.bindingCount || attributeCount!=other.attributeCount) return false;
        if(memcmp((void*)bindings,(void*)other.bindings,bindingCount * sizeof(vk::VertexInputBindingDescription))!=0)
            return false;
        if(memcmp((void*)attributes,(void*)other.attributes, attributeCount * sizeof(vk::VertexInputAttributeDescription))!=0)
            return false;
        return true;
    }

    bool operator==(const vk::PipelineVertexInputStateCreateInfo& other) const
    {
        if(bindingCount!=other.vertexBindingDescriptionCount || attributeCount!=other.vertexAttributeDescriptionCount) return false;

        for (int i = 0; i < bindingCount; i ++)
        {
            int j = 0;
            for(; j < bindingCount; j++)
            {
                if(bindings[i].binding == other.pVertexBindingDescriptions[j].binding)
                {
                    if(bindings[i].inputRate != other.pVertexBindingDescriptions[j].inputRate) return false;
                    if(bindings[i].stride!=other.pVertexBindingDescriptions[j].stride) return false;
                    break;
                }
            }
            if(j == bindingCount) return false;
        }

        for(int i = 0; i < attributeCount; i++)
        {
            int j = 0;
            for(; j < attributeCount; j++)
            {
                if(bindings[i].binding == other.pVertexAttributeDescriptions[j].binding)
                {
                    if(attributes[i].offset!=other.pVertexAttributeDescriptions[j].offset) return false;
                    if(attributes[i].format!=other.pVertexAttributeDescriptions[j].format) return false;
                    if(attributes[i].location!=other.pVertexAttributeDescriptions[j].location) return false;
                    break;
                }
            }
            if(j == attributeCount) return false;
        }

        return true;
    }
};

struct VertexShader;
struct FragmentShader;

#define VK_GRAPHICS_PIPELINE_STATE_DEF_GETTER(state)    private: vk::Pipeline##state##StateCreateInfo _##state##StateInfo{}; \
                                                        public: auto get##state##Info() const{ \
                                                            return  _##state##StateInfo;                         \
                                                        }
struct VulkanGraphicsPipeline
{

    bool compatibleWith(const VulkanPipelineVertexInputStateInfo& vertexInputStateInfo)
    {
        return this->vertexInputStateInfo == vertexInputStateInfo;
    }

    bool compatibleWith(const vk::PipelineVertexInputStateCreateInfo& vertexInputStateInfo)
    {
        return this->vertexInputStateInfo == vertexInputStateInfo;
    }

    bool compatibleWithVertexShader(const std::string& shaderVariantUUID);

    bool compatibleWithFragmentShader(const std::string& shaderVariantUUID);

    bool compatibleWith(const vk::PipelineShaderStageCreateInfo& shaderInfo)
    {
        return false;
    }

    bool compatibleWith(const vk::GraphicsPipelineCreateInfo& createInfo) const
    {
        return false;
    }

    bool operator==(const vk::GraphicsPipelineCreateInfo& createInfo) const
    {
        return compatibleWith(createInfo);
    }

    vk::PipelineLayout getLayout() const
    {
        return pipelineLayout;
    }

    VK_GRAPHICS_PIPELINE_STATE_DEF_GETTER(InputAssembly)
    VK_GRAPHICS_PIPELINE_STATE_DEF_GETTER(ColorBlend)
    VK_GRAPHICS_PIPELINE_STATE_DEF_GETTER(DepthStencil)
    VK_GRAPHICS_PIPELINE_STATE_DEF_GETTER(Rasterization)
    VK_GRAPHICS_PIPELINE_STATE_DEF_GETTER(Multisample)
    VK_GRAPHICS_PIPELINE_STATE_DEF_GETTER(Viewport)
    VK_GRAPHICS_PIPELINE_STATE_DEF_GETTER(Dynamic)

    vk::Pipeline getPipeline() const
    {
        return _pipeline;
    }

    auto getBackendDevice() const
    {
        return device;
    }

    auto getVertexShaderInfo() const
    {
        return _vs;
    }

    auto getFragmentShaderInfo() const
    {
        return _fs;
    }

    auto getVertexInputStateInfo() const
    {
        return vertexInputStateInfo;
    }

    auto getRenderPass() const
    {
        return renderPass;
    }

    auto getPipelineLayout() const
    {
        return pipelineLayout;
    }
private:
    vk::Pipeline _pipeline = VK_NULL_HANDLE;

    friend struct VulkanGraphicsPipelineBuilder;
    VulkanGraphicsPipeline() = default;

    vk::Device device = VK_NULL_HANDLE;

    VertexShader* _vs;

    FragmentShader* _fs;

    VulkanPipelineVertexInputStateInfo vertexInputStateInfo;

    vk::RenderPass renderPass = VK_NULL_HANDLE ;

    vk::PipelineLayout pipelineLayout = VK_NULL_HANDLE ;

    vk::PipelineColorBlendAttachmentState defaultAttachmentState{};
};

/*
 * The reason I use a pipeline builder is that I want to
 * keep the immutability of PSO. In other word, the setters are
 * hidden from the user, once a pipeline object is built, you can
 * only query the information, but you cannot modify it or try to
 * rebuild it again.
 *
 * Builder state is mutable, but pipeline is not.
 * */

#define VK_GRAPHICS_PIPELINE_BUILDER_STATE_DEF_ACCESSOR(state) private : vk::Pipeline##state##StateCreateInfo _##state##StateInfo{}; \
                                                        public : void setStateInfo(const vk::Pipeline##state##StateCreateInfo & info) { \
                                                             _##state##StateInfo = info;                                          \
                                                        }
struct VulkanGraphicsPipelineBuilder
{
    VulkanGraphicsPipelineBuilder(vk::Device device,
                                  VertexShader* vs,
                                  FragmentShader* fs,
                                  vk::PipelineVertexInputStateCreateInfo vertexInputInfo,
                                  vk::RenderPass renderPass);

    VulkanGraphicsPipelineBuilder(vk::Device device,
                                  VertexShader* vs,
                                  FragmentShader* fs,
                                  vk::PipelineVertexInputStateCreateInfo vertexInputInfo,
                                  vk::RenderPass renderPass,
                                  vk::PipelineLayout pipelineLayout)
                                  :VulkanGraphicsPipelineBuilder(device,vs,fs,vertexInputInfo,renderPass)
    {
        if(pipelineLayout == VK_NULL_HANDLE)
        {
            //create empty pipeline layout
            vk::PipelineLayoutCreateInfo emptyLayoutInfo{};
            emptyLayoutInfo.setSetLayoutCount(0);
            _pipelineLayout = device.createPipelineLayout(emptyLayoutInfo);
        }else{
            _pipelineLayout = pipelineLayout;
        }
    }

    template<typename... Args>
    VulkanGraphicsPipelineBuilder(vk::Device device,
                                  VertexShader* vs,
                                  FragmentShader* fs,
                                  vk::PipelineVertexInputStateCreateInfo vertexInputInfo,
                                  vk::RenderPass renderPass, Args... args)
                                  :VulkanGraphicsPipelineBuilder(device,vs,fs,vertexInputInfo,renderPass)
    {

        (setStateInfo(std::move(args)), ...);
    }

    template<typename... Args>
    VulkanGraphicsPipelineBuilder(vk::Device device,
                                  VertexShader* vs,
                                  FragmentShader* fs,
                                  vk::PipelineVertexInputStateCreateInfo vertexInputInfo,
                                  vk::RenderPass renderPass,
                                  vk::PipelineLayout pipelineLayout,
                                  Args... args)
                                  : VulkanGraphicsPipelineBuilder(device,vs,fs,vertexInputInfo,renderPass,pipelineLayout)
    {
        (setStateInfo(std::move(args)), ...);
    }

    /*
     *  Make sure you have set all desired state before calling build
     */
    VulkanGraphicsPipeline build() const;

    VK_GRAPHICS_PIPELINE_BUILDER_STATE_DEF_ACCESSOR(InputAssembly)
    VK_GRAPHICS_PIPELINE_BUILDER_STATE_DEF_ACCESSOR(ColorBlend)
    VK_GRAPHICS_PIPELINE_BUILDER_STATE_DEF_ACCESSOR(DepthStencil)
    VK_GRAPHICS_PIPELINE_BUILDER_STATE_DEF_ACCESSOR(Rasterization)
    VK_GRAPHICS_PIPELINE_BUILDER_STATE_DEF_ACCESSOR(Multisample)
    VK_GRAPHICS_PIPELINE_BUILDER_STATE_DEF_ACCESSOR(Viewport)
    VK_GRAPHICS_PIPELINE_BUILDER_STATE_DEF_ACCESSOR(Dynamic)

private:
    vk::Device _device;
    VertexShader* _vs;
    FragmentShader* _fs;
    VulkanPipelineVertexInputStateInfo _vertexInputInfo;
    vk::RenderPass _renderPass;
    vk::PipelineLayout _pipelineLayout;
    vk::PipelineColorBlendAttachmentState defaultAttachmentState{};
    vk::DynamicState dynamicStates[2]{ vk::DynamicState::eViewport,vk::DynamicState::eScissor };
};


#pragma once

#include "VulkanExtension.h"
#include "Singleton.h"
#include "FrameGraphResourceDef.hpp"

struct FrameGraph;
struct GPUPass;
struct GPURasterizedPass;

using PassFramebufferRecord = std::pair<GPURasterizedPass*, vk::Framebuffer>;

using DescriptorResource = std::variant<vk::Buffer, vk::ImageView, vk::AccelerationStructureKHR>;
using DescriptorResourceProvider = std::function<DescriptorResource(void)>;

using DescriptorSetRecord = std::pair<std::string, vk::DescriptorSet>;
using DescriptorSetRecordList = std::vector<DescriptorSetRecord>;
using DescriptorSetLayoutRecord = std::pair<DescriptorSetLayoutExtended, DescriptorSetRecordList>;

struct PassDataDescriptorSetBaseLayout {
    PassDataDescriptorSetBaseLayout() = default;
    std::vector<vk::DescriptorSetLayoutBinding> bindings;
    // Initial write is incomplete. DstSet need to be set.
    std::vector<vk::WriteDescriptorSet> writes;
    std::shared_ptr<vk::DescriptorImageInfo[]> imgInfos{};
    std::shared_ptr<vk::DescriptorBufferInfo[]> bufInfos{};
};

struct AccessTrackedImage
{
    vk::Image image;
    vk::AccessFlags2 lastAccess;
    vk::PipelineStageFlags2 lastStage;
    vk::ImageLayout lastLayout;
    AccessTrackedImage() {}
    AccessTrackedImage(vk::Image image,
                       vk::AccessFlags2 access,
                       vk::PipelineStageFlags2 stage,
                       vk::ImageLayout layout) : image(image),lastAccess(access),lastStage(stage),lastLayout(layout)
    {

    }
};

struct FrameCoordinator;

struct GPUFrame {

    struct frameGlobalData {
        glm::vec4 time;
        glm::vec4 cursorPos;
    };

    GPUFrame(int threadsNum, DeviceExtended * backendDevice, FrameCoordinator * coordinator);

    GPUFrame(const GPUFrame& other) = delete;

    GPUFrame& operator=(const GPUFrame& other) = delete;

    GPUFrame(GPUFrame&& other) = default;

    GPUFrame& operator=(GPUFrame&& other) = default;

    int workThreadsNum;

    /* Note that allocated command buffer only means we don't need to reallocate them from pool.
     * We still need to record them every frame.
     * The reason we give each thread an allocator instead of command buffer from the same pool is that
     * We don't know how many secondary command buffers one thread may need*/
    std::vector<LinearCachedCommandAllocator> perThreadMainCommandAllocators;

    // Dedicate compute commands only make sense when device indeed has dedicated compute queue.
    // Otherwise, main CommandPool is sufficient since all type of command can submit to it.
    std::vector<LinearCachedCommandAllocator> perThreadDedicatedComputeCommandAllocators;

    DeviceExtended* backendDevice;

    vk::CommandBuffer recordMainQueueCommands(uint32_t avaliableSwapChainImageIdx);

    void copyPresentToSwapChain(vk::CommandBuffer cmd, uint32_t avaliableSwapChainImageIdx);

    vk::ImageView getBackingImageView(const std::string& name) const;
    vk::Image getBackingImage(const std::string& name) const;
    AccessTrackedImage* getBackingTrackedImage(const std::string& name) const;

    vk::DescriptorSet _frameGlobalDescriptorSet;
    VMAObservedBufferMapped<frameGlobalData> _frameGlobalDataBuffer;

    vk::DescriptorSetLayout getFrameGlobalDescriptorSetLayout() const;

    float cursor_x = 0;
    float cursor_y = 0;

    uint32_t frameIdx{};

    vk::Fence executingFence;
    vk::Semaphore imageAvailableSemaphore;
    vk::Semaphore renderFinishSemaphore;

    bool is_executing = false;

    void waitForExecution()
    {
        backendDevice->waitForFences(executingFence, vk::True, std::numeric_limits<uint64_t>::max());
        is_executing = false; //maybe unsafe... would compiler reorder the instructions?
    }

    void lock()
    {
        backendDevice->resetFences(executingFence);
        is_executing = true;
    }

    FrameCoordinator* frameCoordinator;

    /* We make framebuffer to be frame-exclusive because:
       1. It's a little tricky to figure out whether a framebuffer can be shared between frames
       2. framebuffer object is really cheap
       */
    std::vector<PassFramebufferRecord> framebuffers;

    vk::Framebuffer getFramebufferFor(GPURasterizedPass* pass)
    {
        for (const auto& record : framebuffers)
        {
            if (record.first == pass)
            {
                return record.second;
            }
        }
        return vk::Framebuffer{};
    }

    void setFramebufferFor(GPURasterizedPass* pass, vk::Framebuffer frameBuffer)
    {
        for (auto& record : framebuffers)
        {
            if (record.first == pass)
            {
                record.second = frameBuffer;
                return;
            }
        }
    }

    std::unordered_map<std::string, vk::ImageView> backingImageViews;
    std::unordered_map<std::string, AccessTrackedImage*> backingImages;
    std::unordered_map<std::string, vk::DescriptorSet> descriptorSets;

    vk::ImageView presentImageView{};
    VMAImage presentVMAImage{};
    mutable AccessTrackedImage presentImage;

    void createPresentImage();

    vk::DescriptorSet getManagedDescriptorSet(const std::string& identifier) const;
};

template<typename T>
struct InFlightObservedBufferMapped;

struct FrameCoordinator : Singleton<FrameCoordinator>
{
    void init(DeviceExtended* device, int num_frames_in_flight);

    template<typename T>
    InFlightObservedBufferMapped<T> allocateInFlightObservedBufferMapped(VkBufferUsageFlagBits);

    vk::DescriptorSetLayout _frameGlobalDescriptorSetLayout;
    vk::PipelineLayout _frameLevelPipelineLayout;
    vk::DescriptorPool _frameGlobalDescriptorSetPool;

    std::unordered_map<std::string, vk::ImageView> allBackingImageViews;
    std::unordered_map<std::string, VMAImage> allBackingImages;
    std::unordered_map<std::string, std::unique_ptr<AccessTrackedImage>> allBackingTrackedImages;
    std::vector<vk::Sampler> samplers;

    VMAImage createVMAImage(FrameGraphTextureDescription* textureDesc) const;
    vk::ImageView createVKImageView(FrameGraphTextureDescription* textureDesc, const FrameGraphResourceAccessInfo& accessInfo,vk::Image image) const;

    void createBackingImage(FrameGraphTextureDescription* textureDesc, bool used_in_flight);
    void createBackingImageView(FrameGraphTextureDescription* textureDesc,const FrameGraphResourceAccessInfo& accessInfo, bool used_in_flight);

    vk::ImageView getBackingImageView(const std::string& name) const {
        return allBackingImageViews.find(name)->second;
    }
    vk::Image getBackingImage(const std::string& name) const {
        return allBackingImages.find(name)->second.image;
    }

    /*
      A discriptorSet can be shared only when:
      1. All the discriptors in the set point to shared resource
      2. There is no need to update the discriptorSet frequently

      Sometimes we do need to updateDescriptorSet when it's still in used for the previous frames. We refer
      so descriptorSet as out-of-date, and will allocate new one for the current frames will keep the old one till
      the previous frame is finished.
    */
    std::vector<DescriptorSetLayoutRecord> managedDescriptorSetLayouts;
    std::vector<vk::DescriptorPool> managedDescriptorPools;
    std::vector<VulkanWriteDescriptorSet> pendingWriteDescriptorSets;
    // When descriptorSet is managed but not allocated yet, writeDescriptorSet operation will be pending.
    std::vector<std::pair<std::string, VulkanWriteDescriptorSet>> pendingAllocateWriteDescriptorSets;

    vk::DescriptorSetLayout getFrameGlobalDescriptorSetLayout() {
        return _frameGlobalDescriptorSetLayout;
    }

    vk::DescriptorSetLayout manageSharedDescriptorSetAOT(std::string&& name, const std::vector<vk::DescriptorSetLayoutBinding>& bindings);
    vk::DescriptorSetLayout manageSharedDescriptorSetAOT(std::string&& name, std::vector<std::tuple<vk::DescriptorType, uint32_t, vk::ShaderStageFlags>>&& bindings);

    vk::DescriptorSetLayout manageInFlightDescriptorSetAOT(std::string&& name, const std::vector<vk::DescriptorSetLayoutBinding>& bindings);
    vk::DescriptorSetLayout manageInFlightDescriptorSetAOT(std::string&& name, std::vector<std::tuple<vk::DescriptorType, uint32_t, vk::ShaderStageFlags>>&& bindings);

    DescriptorSetRecord findManagedDescriptorSetRecord(const std::string& name);

private:
    void updateDescriptorSetAOT(std::string&& name, VulkanWriteDescriptorSet);
public:
    void updateSharedDescriptorSetAOT(std::string&& name, VulkanWriteDescriptorSet);
    void updateSharedDescriptorSetAOT(std::string&& name, const std::vector<VulkanWriteDescriptorSet>&);
    void updateSharedDescriptorSetAOT(std::string&& name, uint32_t dstBinding, DescriptorResourceProvider);

    void updateInFlightDescriptorSetAOT(std::string&& name, std::function<VulkanWriteDescriptorSet(GPUFrame*)>);
    void updateInFlightDescriptorSetAOT(std::string&& name, std::function<std::vector<VulkanWriteDescriptorSet>(GPUFrame*)>);
    void updateInFlightDescriptorSetAOT(std::string&& name, uint32_t dstBinding, vk::DescriptorType descriptorType, std::function<DescriptorResource(GPUFrame*)>);

    vk::DescriptorSet getManagedDescriptorSet(std::string&& name) const;

    void prepareDescriptorSetsAOT();
    void allocateDescriptorSetsAOT();
    void doUpdateDescriptorSetsAOT();

    DeviceExtended* backendDevice;
    std::vector<GPUFrame*> inFlightframes;

    FrameGraph* frameGraph;

    void compileFrameGraphAOT();

    uint32_t current_frame_idx = 0;

    void waitForCurrentFrame()
    {
        inFlightframes[current_frame_idx]->waitForExecution();
    }

    GPUFrame* currentFrame()
    {
        return inFlightframes[current_frame_idx];
    }

    void acquireNextFrame()
    {
        current_frame_idx = (current_frame_idx + 1) % inFlightframes.size();
    }

    enum class Event
    {
        SWAPCHAIN_RESIZE
    };

    void update(Event event);
};

/*
 * Note that the content of this kind of buffer is not 'consistent' between frames 
 * which means that client cannot assume that the content for current frame is same as 
 * the previous frame despite no write operations;
 */
template<typename T>
struct InFlightObservedBufferMapped
{
    const T* operator->() {
        return mappedBuffers[coordinator->current_frame_idx].operator->();
    }

    //Only call this when you're unsure whether current frame is safe to modify
    T* writeBlock() {
        coordinator->waitForCurrentFrame();
        return mappedBuffers[coordinator->current_frame_idx].operator->();
    }

    //Only call this when you're unsure whether current frame is safe to modify
    void wirteBlock(const T& otherData) {
        coordinator->waitForCurrentFrame();
        mappedBuffers[coordinator->current_frame_idx] = otherData;
    }

    void operator=(const T& otherData)
    {
        mappedBuffers[coordinator->current_frame_idx] = otherData;
    }

    VkBuffer getCurrentFrameBuffer() const
    {
        return mappedBuffers[coordinator->current_frame_idx].getBuffer();
    }

    VkBuffer getBufferFor(uint32_t frameIdx) const
    {
        return mappedBuffers[frameIdx].getBuffer();
    }

    friend struct DeviceExtended;
    friend struct FrameCoordinator;
private:
    std::vector<VMAObservedBufferMapped<T>> mappedBuffers;
    FrameCoordinator* coordinator;
};

template<typename T>
InFlightObservedBufferMapped<T> FrameCoordinator::allocateInFlightObservedBufferMapped(VkBufferUsageFlagBits usage)
{
    InFlightObservedBufferMapped<T> inflightMappedBuffer;
    inflightMappedBuffer.coordinator = this;
    for (int i = 0; i < inFlightframes.size(); i++)
    {
        inflightMappedBuffer.mappedBuffers.emplace_back(backendDevice->allocateObservedBufferPull<T>(usage).value());
    }
    return inflightMappedBuffer;
}



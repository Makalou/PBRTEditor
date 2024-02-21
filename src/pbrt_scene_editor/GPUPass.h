//
// Created by 王泽远 on 2024/1/16.
//

#ifndef PBRTEDITOR_GPUPASS_H
#define PBRTEDITOR_GPUPASS_H

#include <vulkan/vulkan.hpp>

#include "RenderScene.h"
#include <unordered_map>
#include <unordered_set>
#include "VulkanExtension.h"
#include "ShaderManager.h"

struct GPUPass;
struct GPUFrame;
struct PassResourceDescriptionBase;

//we use pointer as handle... may be not best idea
template<typename T>
struct pointer_view
{
    T * pointer;

    pointer_view() = default;

    pointer_view(T* ptr) : pointer(ptr) {}

    pointer_view(const pointer_view& other) : pointer(other.pointer) {}

    pointer_view(pointer_view&& other) noexcept : pointer(other.pointer){}

    pointer_view& operator=(const pointer_view& other) {
        pointer = other.pointer;
        return *this;
    }

    pointer_view& operator=(pointer_view&& other) noexcept {
        pointer = other.pointer;
        return *this;
    }

    T* operator->() {
        return pointer;
    }

    explicit operator bool() const {
        return pointer != nullptr;
    }

    ~ pointer_view() = default;
};
using GPUPassHandle = pointer_view<GPUPass>;
using PassResourceHandle = pointer_view<PassResourceDescriptionBase>;

enum PassResourceType
{
    Attachment,
    Texture,
    Buffer
};

bool isDepthStencilFormat(vk::Format format);

struct PassResourceDescriptionBase
{
    int ref_count = 0;
    GPUPassHandle producer{};
    //used to link an input to its output resource
    PassResourceHandle outputHandle{};
    // uuid
    std::string name;

    virtual PassResourceType getType() const = 0;

    explicit PassResourceDescriptionBase(const std::string& nam) : name(nam)
    {

    }

    virtual ~PassResourceDescriptionBase()= default;
};

namespace PassAttachmentExtent
{
    struct SwapchainRelative
    {
        SwapchainRelative() = default;

        SwapchainRelative(float width_scale, float height_scale) : wScale(width_scale), hScale(height_scale){}

        float wScale = 1.0f;
        float hScale = 1.0f;
    };

    struct Absolute
    {
        Absolute(int width, int height) : w(width), h(height) {}

        int w;
        int h;
    };
}

using PassAttachmentExtentType = std::variant<PassAttachmentExtent::SwapchainRelative,PassAttachmentExtent::Absolute>;

/* Used to determine the render pass and framebuffer composition of a given node.
 * attachments can be defined both for inputs and outputs.
 * This is needed to continue working on a resource in multiple nodes.
 * When we specific an attachment as the input, we are actually saying that we want to continuously work
 * on that attachment. Implicitly, it implies that we want to preserve the content in the attachment.
 *
 * For example, after we run a depth prepass, we want to load the depth data and use it
 * during the g-buffer pass to avoid shading pixels for objects that are hidden behind other objects.
 * We achieve this by declaring the depth map as the input attachment of g-buffer pass and set the depth state
 * of g-buffer pass's pipeline.
 * */
struct PassAttachmentDescription : PassResourceDescriptionBase
{
    vk::Format format = vk::Format::eUndefined;
    //int width = 0;
    //int height = 0;
    PassAttachmentExtentType extent;
    vk::AttachmentLoadOp loadOp{};
    vk::AttachmentStoreOp storeOp{};
    //If the format has depth and/or stencil components,
    // loadOp and storeOp apply only to the depth data,
    // while stencilLoadOp and stencilStoreOp define how the stencil data is handled.
    vk::AttachmentLoadOp stencilLoadOp{};
    vk::AttachmentStoreOp stencilStoreOp{};
    vk::ImageLayout initialLayout = vk::ImageLayout::eUndefined;
    vk::ImageLayout finalLayout = vk::ImageLayout::eUndefined;

    PassAttachmentDescription(const std::string& name, vk::Format format, int width, int height, vk::AttachmentLoadOp loadOp, vk::AttachmentStoreOp storeOp)
    : PassResourceDescriptionBase(name)
    {
        this->format = format;
        //this->width = width;
        //this->height = height;
        this->extent = PassAttachmentExtent::Absolute(width, height);
        this->loadOp = loadOp;
        this->storeOp = storeOp;
    }

    PassAttachmentDescription(const std::string& name, vk::Format format,PassAttachmentExtentType && size, vk::AttachmentLoadOp loadOp, vk::AttachmentStoreOp storeOp)
        : PassResourceDescriptionBase(name)
    {
        this->format = format;
        //this->width = width;
        //this->height = height;
        this->extent = size;
        this->loadOp = loadOp;
        this->storeOp = storeOp;
    }

    PassAttachmentDescription(const std::string& name,vk::AttachmentLoadOp loadOp, vk::AttachmentStoreOp storeOp)
            : PassResourceDescriptionBase(name)
    {
        this->loadOp = loadOp;
        this->storeOp = storeOp;
        this->extent = PassAttachmentExtent::Absolute(0,0);
    }

    PassResourceType getType() const override
    {
        return Attachment;
    }
    
    vk::Extent2D getExtent(const vkb::Swapchain& swapchain)
    {
        vk::Extent2D size;
        std::visit([&](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, PassAttachmentExtent::SwapchainRelative>) {
                size.width = swapchain.extent.width * arg.wScale;
                size.height = swapchain.extent.height * arg.hScale;
            }
            else if constexpr (std::is_same_v<T, PassAttachmentExtent::Absolute>) {
                size.width = arg.w;
                size.height = arg.h;
            }
        }, extent);

        return size;
    }
};

/* Used to distinguish images from attachments.
 * An attachment has to be part of the definition of the render pass and framebuffer for a node,
 * while a texture is read during the pass and is part of a shader data definition.
 * This distinction is also important to determine which images need to be transitioned to a
 * different layout and require an image barrier.
 * We don’t need to specify the size and format of the texture here, as we had already done so
 * when we first defined the resource as an output.*/
struct PassTextureDescription : PassResourceDescriptionBase
{
    vk::Format format = vk::Format::eUndefined;
    int width = 0;
    int height = 0;

    PassTextureDescription(const std::string& name, vk::Format format, int width, int height) : PassResourceDescriptionBase(name)
    {
        this->format = format;
        this->width = width;
        this->height = height;
    }

    explicit PassTextureDescription(const std::string& name) : PassResourceDescriptionBase(name)
    {

    }

    PassResourceType getType() const override
    {
        return Texture;
    }
};

/* This type represents a storage buffer that we can write to or read from. As with
 * textures, we will need to insert memory barriers to ensure the writes from a previous pass are
 * completed before accessing the buffer data in another pass.*/
struct PassBufferDescription : PassResourceDescriptionBase
{

};

/*This type is used exclusively to ensure the right edges between nodes are computed
 * without creating a new resource.*/
struct PassReferenceDescription : PassResourceDescriptionBase
{

};

struct GPUPass
{
    // Used when reference pass output
    // When reference the output of a pass, one should use name::
    // as the name space.
    std::string _name;

    explicit GPUPass(std::string  name) : _name(std::move(name))
    {

    }

    virtual ~GPUPass() = default;

    //DeviceExtended* backend_device;
    std::vector<std::unique_ptr<PassResourceDescriptionBase>> inputs;
    std::vector<std::unique_ptr<PassResourceDescriptionBase>> outputs;
    std::vector<std::unique_ptr<PassResourceDescriptionBase>> inouts;
    std::vector<GPUPassHandle> edges;
    vk::DescriptorSetLayout passInputDescriptorSetLayout;
    bool enabled = true;
    /*
     * For some passes, the shaders, pipelines to use can be determined Ahead of Time,
     * And would never change during the whole applications time.
     * This method will usually be called only once when frame graph was first compiled.
     * Ideally one should do as many ahead of time jobs as possible. Such as build all the shaders and pipelines
     * that would be used by the pass, prepare other immutable per pass resources.
     */
    virtual void prepareAOT(GPUFrame* frame) = 0;

    /*
     * For some passes, it's hard or impossible to decide which shaders or pipeline to use ahead of time.
     * And maybe new shader variants can appear at every frame.
     * Also per pass data can change every frame, so this function also plays the role of 'Prepare'
     * */
    virtual void prepareIncremental(const GPUFrame* frame){};

    void addInput(std::unique_ptr<PassResourceDescriptionBase> resource)
    {
        inputs.push_back(std::move(resource));
    }

    void addOutput(std::unique_ptr<PassResourceDescriptionBase> resource)
    {
        outputs.push_back(std::move(resource));
    }

    void addInOut(std::unique_ptr<PassResourceDescriptionBase> resource)
    {

    }

    template<typename T,class... Args>
    void addInput(Args&& ... args)
    {
        //static_assert(!std::is_same_v<T, PassAttachmentDescription>);
        inputs.emplace_back(std::make_unique<T>(args...));
    }

    template<typename T,class... Args>
    void addOutput(Args&& ... args)
    {
        outputs.emplace_back(std::make_unique<T>(args...));
    }

    template<typename T, class... Args>
    void addInOut(const std::string& inputName, const std::string& outputName,Args&& ... args)
    {
        inouts.emplace_back(std::make_unique<T>(inputName + "->" + outputName,args...));
    }
};

struct FrameExternalResourceImmutable
{
public:
    explicit FrameExternalResourceImmutable(vk::Buffer res) : resource(res){

    }
    vk::Buffer get()
    {
        return resource;
    }
private:
    const vk::Buffer resource;
};

template<int max_frame_in_flight>
struct FrameExternalBufferMutable
{
public:
    explicit FrameExternalBufferMutable(std::array<vk::Buffer,max_frame_in_flight> resource) : in_flight_resource_arr(resource){}

    vk::Buffer get(int frameIdx)
    {
        return in_flight_resource_arr[frameIdx];
    }
private:
    std::array<vk::Buffer,max_frame_in_flight> in_flight_resource_arr;
};

template<int max_frame_in_flight>
struct FrameExternalBufferMutableOpaque
{
public:
    explicit FrameExternalBufferMutableOpaque(vk::Buffer resource) : res(resource){}

    void access(int frameIdx,const std::function<void(int frameIdx, vk::Buffer buffer)>& accessor)
    {
        accessor(frameIdx,res);
    }
private:
    const vk::Buffer res;
};

struct GPUComputePass : GPUPass
{
    std::unordered_map<std::string, vk::Pipeline> computePipelines;
    std::unordered_map<std::string, vk::PipelineLayout> pipelineLayouts;
    std::unordered_map<std::string, vk::ShaderModule> shaders;
};

struct GPURasterizedPass : GPUPass
{
    virtual void record(vk::CommandBuffer cmdBuf, const GPUFrame* frame) = 0;
    void beginPass(vk::CommandBuffer cmdBuf);

    void endPass(vk::CommandBuffer cmdBuf);

    void buildRenderPass(const DeviceExtended& device, GPUFrame* frame);

    void buildFrameBuffer(GPUFrame* frame);

    vk::RenderPass renderPass;
    vk::Framebuffer frameBuffer;
    vk::FramebufferCreateInfo framebufferCreateInfo{};
    vk::RenderPassCreateInfo renderPassCreateInfo{};
    std::vector<vk::AttachmentDescription> attachmentDescriptions{};
    std::vector<vk::AttachmentReference> colorAttachmentRefs;
    vk::AttachmentReference depthStencilAttachmentRef{};
    vk::SubpassDescription subpassInfo{};

    std::vector<VulkanGraphicsPipeline> graphicsPipelines;
    vk::PipelineLayout passBaseLayout;
    vk::PipelineLayoutCreateInfo passBaseLayoutInfo{};
    //We should guarantee that each pipelineLayouts must be compatible with passBaseLayout
    std::vector<std::pair<std::vector<vk::DescriptorSetLayout>, vk::PipelineLayout>> pipelineLayouts;

    GPURasterizedPass() = default;
    explicit GPURasterizedPass(const std::string& name) : GPUPass(name){};

    virtual ~GPURasterizedPass()
    {

    }
};

struct TransferPass : GPUPass
{

};

struct GPURayTracingPass : GPUPass
{
    std::vector<std::pair<vk::Pipeline,vk::RayTracingPipelineCreateInfoKHR>> computePipelines;
    std::vector<vk::ShaderModule> shaders;
};

struct GPUFrame {
    struct frameGlobalData {
        glm::vec4 time;
        glm::vec4 cursorPos;
    };

    GPUFrame(int threadsNum, const std::shared_ptr<DeviceExtended> &backendDevice);

    GPUFrame(const GPUFrame &other) = delete;

    GPUFrame &operator=(const GPUFrame &other) = delete;

    GPUFrame(GPUFrame &&other) = default;

    GPUFrame &operator=(GPUFrame &&other) = default;

    int workThreadsNum;
    /* Note that allocated command buffer only means we don't need to reallocate them from pool.
     * We still need to record them every frame.
     * The reason we give each thread an allocator instead of command buffer from the same pool is that
     * We don't know how many secondary command buffers one thread may need*/
    std::vector<LinearCachedCommandAllocator> perThreadMainCommandAllocators;

    // Dedicate compute commands only make sense when device indeed has dedicated compute queue.
    // Otherwise, main CommandPool is sufficient since all type of command can submit to it.
    std::vector<LinearCachedCommandAllocator> perThreadDedicatedComputeCommandAllocators;

    std::shared_ptr<DeviceExtended> backendDevice;

    vk::CommandBuffer recordMainQueueCommands();

    void registerRasterizedGPUPass(std::unique_ptr<GPURasterizedPass> &&pass) {
        for (const auto &reg_pass: _rasterPasses) {
            if (pass->_name == reg_pass->_name) {
                throw std::invalid_argument("Pass has been registered.");
            }
        }
        _rasterPasses.emplace_back(std::move(pass));
    }

    struct PassDataDescriptorSetBaseLayout {
        PassDataDescriptorSetBaseLayout()
        = default;

        std::vector<vk::DescriptorSetLayoutBinding> bindings;
        // Initial write is incomplete. DstSet need to be set.
        std::vector<vk::WriteDescriptorSet> writes;
        std::shared_ptr<vk::DescriptorImageInfo[]> imgInfos{};
        std::shared_ptr<vk::DescriptorBufferInfo[]> bufInfos{};

        //PassDataDescriptorSetBaseLayout(const PassDataDescriptorSetBaseLayout &other) = delete;

        //PassDataDescriptorSetBaseLayout &operator=(const PassDataDescriptorSetBaseLayout &other) = delete;

        /*PassDataDescriptorSetBaseLayout(PassDataDescriptorSetBaseLayout &&other) noexcept {
            bindings = std::move(other.bindings);
            writes = std::move(other.writes);
            imgInfos = std::move(other.imgInfos);
            bufInfos = std::move(other.bufInfos);
        }

        PassDataDescriptorSetBaseLayout &operator=(PassDataDescriptorSetBaseLayout &&other) noexcept {
            if (this != &other) {
                bindings = std::move(other.bindings);
                writes = std::move(other.writes);
                imgInfos = std::move(other.imgInfos);
                bufInfos = std::move(other.bufInfos);
            }
            return *this;
        }*/
    };

    /*
     * Get base layout(include previous pass attachment as input texture).
     * Note that writeDescriptorSets cannot be directly used : caller must
     * manually set the dstSet.
     * */
    PassDataDescriptorSetBaseLayout getPassDataDescriptorSetBaseLayout(GPUPass *pass) const;

    //https://app.diagrams.net/#G1gIpgDwpK7Vyhzypl7A_RbQFETWhS1_1q
    void compileAOT();

    enum class Event
    {
        SWAPCHAIN_RESIZE
    };

    void update(Event event);

    auto createBackingImage(PassAttachmentDescription* attachmentDesc)
    {
        VMAImage image;
        /*todo frame graph should some how determine whether an attachment need to create with sample bit or not.
         * For color attachment, it seems that every attachment need to be sampled by further passes, except swapchainimage
         * For depth attachment, it might not be the case.*/
        bool sampled_need = true;
        auto attachmentExtent = attachmentDesc->getExtent(backendDevice->_swapchain);
        if (isDepthStencilFormat(attachmentDesc->format))
        {
            image = backendDevice->allocateVMAImageForDepthStencilAttachment(
                    static_cast<VkFormat>(attachmentDesc->format), attachmentExtent.width, attachmentExtent.height, sampled_need).value();
        }
        else {
            image = backendDevice->allocateVMAImageForColorAttachment(
                    static_cast<VkFormat>(attachmentDesc->format), attachmentExtent.width, attachmentExtent.height, sampled_need).value();
        }

        return image;
    }

    auto createBackingImageView(PassAttachmentDescription* attachmentDesc, vk::Image image)
    {
        vk::ImageViewCreateInfo imageViewInfo{};
        imageViewInfo.image = image;
        imageViewInfo.viewType = vk::ImageViewType::e2D;
        imageViewInfo.format = attachmentDesc->format;
        vk::ComponentMapping componentMapping{};
        componentMapping.a = vk::ComponentSwizzle::eA;
        componentMapping.r = vk::ComponentSwizzle::eR;
        componentMapping.g = vk::ComponentSwizzle::eG;
        componentMapping.b = vk::ComponentSwizzle::eB;
        imageViewInfo.components = componentMapping;
        vk::ImageSubresourceRange subresourceRange;
        if (isDepthStencilFormat(attachmentDesc->format))
        {
            subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
        }
        else {
            subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        }
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = 1;
        subresourceRange.baseArrayLayer = 0;
        subresourceRange.layerCount = 1;
        imageViewInfo.subresourceRange = subresourceRange;
        return backendDevice->createImageView(imageViewInfo);
    }

    vk::ImageView getBackingImageView(const std::string &name) const {
        return backingImageViews.find(name)->second;
    }

    int allocateSingleDescriptorSet(vk::DescriptorSetLayout layout) const {

    }

    void getAllocatedDescriptorSet(int handle, const std::function<void(vk::DescriptorSet)> &callback) const {

    }

    void connectResources(PassResourceDescriptionBase *output, PassResourceDescriptionBase *input);

    using DescriptorSetRecord = std::pair<std::string,vk::DescriptorSet>;
    using DescriptorSetRecordList = std::vector<DescriptorSetRecord>;
    using DescriptorSetLayoutRecord = std::pair<DescriptorSetLayoutExtended,DescriptorSetRecordList>;
    using DescriptorSetCallback = std::function<void(const vk::DescriptorSet &)>;

    vk::DescriptorSetLayout manageDescriptorSet(std::string && name, const std::vector<vk::DescriptorSetLayoutBinding> & bindings);

    void getManagedDescriptorSet(std::string && setName, const std::function<void(vk::DescriptorSet)>& cb);

    void prepareDescriptorSetsAOT();

    vk::DescriptorSet getManagedDescriptorSet(std::string && name) const;

    void managePassInputDescriptorSet(GPUPass * pass);

    PassAttachmentDescription* swapchainAttachment;
    std::vector<int> sortedIndices;
    std::vector<std::unique_ptr<GPURasterizedPass>> _rasterPasses;
    std::unordered_map<std::string,vk::ImageView> backingImageViews;
    std::unordered_map<std::string,VMAImage> backingImages;
    std::vector<vk::Sampler> samplers;

    vk::DescriptorSet _frameGlobalDescriptorSet;
    vk::DescriptorSetLayout _frameGlobalDescriptorSetLayout;
    vk::DescriptorPool _frameGlobalDescriptorSetPool;
    VMAObservedBufferMapped<frameGlobalData> _frameGlobalDataBuffer;
    vk::PipelineLayout _frameLevelPipelineLayout;

    std::vector<DescriptorSetLayoutRecord> managedDescriptorSetLayouts;
    std::vector<vk::DescriptorPool> managedDescriptorPools;
    std::unordered_map<std::string,std::vector<DescriptorSetCallback>> descriptorSetCallbackMap;

    float x = 0;
    float y = 0;
    int frameIdx{};
};

#endif //PBRTEDITOR_GPUPASS_H

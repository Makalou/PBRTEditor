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
    Texture,
    Buffer
};

bool isDepthStencilFormat(vk::Format format);

namespace PassResourceAccess
{
    struct Write
    {

    };

    struct RenderTarget
    {
        vk::AttachmentLoadOp loadOp;
        vk::AttachmentStoreOp storeOp;
    };

    struct Read
    {

    };

    struct Sample
    {
        bool need_mimap;
    };

    using Access = std::variant<Write, RenderTarget, Read, Sample>;
}

struct PassResourceAccessInfo
{
    GPUPass* pass;
    PassResourceAccess::Access accessType;
};

struct PassResourceDescriptionBase
{
    // uuid
    std::string name;

    virtual PassResourceType getType() const = 0;

    explicit PassResourceDescriptionBase(const std::string& nam) : name(nam)
    {

    }

    virtual ~PassResourceDescriptionBase()= default;

    virtual void resolveBarriers(GPUFrame*) = 0;

    std::vector<PassResourceAccessInfo> accessList;
};

namespace PassTextureExtent
{
    struct SwapchainRelative
    {
        SwapchainRelative() = default;

        SwapchainRelative(float width_scale, float height_scale) : wScale(width_scale), hScale(height_scale) {}

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

using PassTextureExtentType = std::variant<PassTextureExtent::SwapchainRelative, PassTextureExtent::Absolute>;

/* Deprecated:
 * Used to distinguish images from attachments.
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
    PassTextureExtentType extent;

    PassTextureDescription(const std::string& name, vk::Format format, int width, int height) : PassResourceDescriptionBase(name)
    {
        this->format = format;
        this->width = width;
        this->height = height;
        this->extent = PassTextureExtent::Absolute(width, height);
    }

    PassTextureDescription(const std::string& name, vk::Format format, PassTextureExtentType&& ext) : PassResourceDescriptionBase(name)
    {
        this->format = format;
        this->width = width;
        this->height = height;
        this->extent = ext;
    }

    explicit PassTextureDescription(const std::string& name) : PassResourceDescriptionBase(name)
    {

    }

    PassResourceType getType() const override
    {
        return Texture;
    }

    vk::Extent2D getExtent(const vkb::Swapchain& swapchain)
    {
        vk::Extent2D size;
        std::visit([&](auto&& ext) {
            using T = std::decay_t<decltype(ext)>;

            if constexpr (std::is_same_v<T, PassTextureExtent::SwapchainRelative>) {
                size.width = swapchain.extent.width * ext.wScale;
                size.height = swapchain.extent.height * ext.hScale;
            }
            else if constexpr (std::is_same_v<T, PassTextureExtent::Absolute>) {
                size.width = ext.w;
                size.height = ext.h;
            }
            }, extent);

        return size;
    }

    bool need_sample() const
    {
        for (auto& access : accessList)
        {
            if (std::holds_alternative<PassResourceAccess::Sample>(access.accessType))
            {
                return true;
            }
        }
        return false;
    }

    bool use_as_attachment() const
    {
        for (auto& access : accessList)
        {
            if (std::holds_alternative<PassResourceAccess::RenderTarget>(access.accessType))
            {
                return true;
            }
        }
        return false;
    }

    bool need_mipmap() const
    {
        for (auto& access : accessList)
        {
            if (std::holds_alternative<PassResourceAccess::Sample>(access.accessType))
            {
                auto sample = std::get<PassResourceAccess::Sample>(access.accessType);
                if (sample.need_mimap)
                {
                    return true;
                }
            }
        }
        return false;
    }

    void resolveBarriers(GPUFrame* frame) override;
};

/* This type represents a storage buffer that we can write to or read from. As with
 * textures, we will need to insert memory barriers to ensure the writes from a previous pass are
 * completed before accessing the buffer data in another pass.*/
struct PassBufferDescription : PassResourceDescriptionBase
{
    void resolveBarriers(GPUFrame* frame) override;
};

using PassTexture = PassTextureDescription;
using PassBuffer = PassBufferDescription;

enum GPUPassType
{
    Graphics,
    Compute
};

struct PassActionContext
{
    uint32_t pipelineIdx;
    std::vector < std::variant<std::string, vk::DescriptorSet>> descriptorSets;
    uint32_t firstSet;
    std::function<void(vk::CommandBuffer, uint32_t)> action;
};

struct GPUPass
{
    // Used when reference pass output
    // When reference the output of a pass, one should use name::
    // as the name space.
    std::string _name;

    explicit GPUPass(std::string name) : _name(std::move(name))
    {

    }

    virtual ~GPUPass() = default;

    virtual GPUPassType getType() const = 0;

    std::vector<PassResourceDescriptionBase*> reads;
    std::vector<PassResourceDescriptionBase*> writes;
    std::vector<PassTextureDescription*> samples;

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
    virtual void prepareIncremental(GPUFrame* frame){};

    void read(PassTextureDescription* texture)
    {

    }

    void write(PassTextureDescription* texture)
    {

    }

    void readwrite(PassTextureDescription* texture)
    {

    }

    void sample(PassTextureDescription* texture, bool mipmap = false)
    {
        PassResourceAccessInfo accessInfo{};
        accessInfo.pass = this;
        accessInfo.accessType = PassResourceAccess::Sample{ mipmap };
        texture->accessList.emplace_back(std::move(accessInfo));
        this->samples.emplace_back(texture);
    }

    virtual bool checkWrite(PassResourceDescriptionBase* res) = 0;
    virtual bool checkRead(PassResourceDescriptionBase* res) = 0;

    virtual bool fillMemBarrierInfo(PassResourceAccess::Access accessType, PassBufferDescription* buffer,vk::MemoryBarrier2& barrier) = 0;
    virtual bool fillImgBarrierInfo(PassResourceAccess::Access accessType, PassTextureDescription* texture,
        vk::PipelineStageFlags2& stageMask, vk::AccessFlags2& accessMask, vk::ImageLayout& layout) = 0;

    std::vector<vk::MemoryBarrier2> memoryBarriers;
    std::vector<vk::BufferMemoryBarrier2> bufferMemoryBarriers;
    std::vector<vk::ImageMemoryBarrier2> imageMemoryBarriers;

    // Insert the pipeline barrier **Before** the command is record
    void insertPipelineBarrier(vk::CommandBuffer cmdBuf)
    {
        if (!memoryBarriers.empty() || !bufferMemoryBarriers.empty() || !imageMemoryBarriers.empty())
        {
            vk::DependencyInfo dependencyInfo{};
            dependencyInfo.setDependencyFlags(vk::DependencyFlagBits::eByRegion);
            dependencyInfo.setMemoryBarriers(memoryBarriers);
            dependencyInfo.setBufferMemoryBarriers(bufferMemoryBarriers);
            dependencyInfo.setImageMemoryBarriers(imageMemoryBarriers);
            cmdBuf.pipelineBarrier2(dependencyInfo);
        }
    }

    std::vector<PassActionContext> actionContextQueue;

    void virtual record2(vk::CommandBuffer cmd, GPUFrame* frame) = 0;
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
    GPUComputePass() = delete;
    explicit GPUComputePass(const std::string& name) : GPUPass(name) {};

    virtual ~GPUComputePass()
    {

    }

    virtual GPUPassType getType() const override
    {
        return GPUPassType::Compute;
    }

    bool checkWrite(PassResourceDescriptionBase* res) override
    {
        return false;
    }

    bool checkRead(PassResourceDescriptionBase* res) override
    {
        return false;
    }

    bool fillMemBarrierInfo(PassResourceAccess::Access accessType, PassBufferDescription* buffer, vk::MemoryBarrier2& barrier) override
    {
        return false;
    }
    bool fillImgBarrierInfo(PassResourceAccess::Access accessType, PassTextureDescription* texture,
        vk::PipelineStageFlags2& stageMask, vk::AccessFlags2& accessMask, vk::ImageLayout& layout) override
    {
        assert(std::holds_alternative<PassResourceAccess::Read>(accessType) || 
            std::holds_alternative<PassResourceAccess::Sample>(accessType) || 
            std::holds_alternative<PassResourceAccess::Write>(accessType));

        stageMask = vk::PipelineStageFlagBits2::eComputeShader;
        if(std::holds_alternative<PassResourceAccess::Read>(accessType))
        {
            accessMask = vk::AccessFlagBits2::eShaderRead;
            layout = vk::ImageLayout::eShaderReadOnlyOptimal;
            return true;
        }
        if (std::holds_alternative<PassResourceAccess::Sample>(accessType))
        {
            accessMask = vk::AccessFlagBits2::eShaderSampledRead;
            layout = vk::ImageLayout::eShaderReadOnlyOptimal;
            return true;
        }
        if (std::holds_alternative<PassResourceAccess::Write>(accessType))
        {
            accessMask = vk::AccessFlagBits2::eShaderWrite;
            layout = vk::ImageLayout::eGeneral;
            return true;
        }

        return false;
    }

    void record2(vk::CommandBuffer cmd, GPUFrame* frame) override;

    std::unordered_map<std::string, vk::Pipeline> computePipelines;
};

struct RenderTarget
{
    vk::AttachmentLoadOp loadOp;
    vk::AttachmentStoreOp storeOp;
    PassTexture* texture;
};

struct GPURasterizedPass : GPUPass
{
    void beginPass(vk::CommandBuffer cmdBuf);

    void endPass(vk::CommandBuffer cmdBuf);

    void buildRenderPass(GPUFrame* frame);

    void buildFrameBuffer(GPUFrame* frame);

    std::vector<RenderTarget> renderTargets;

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

    GPURasterizedPass() = delete;
    explicit GPURasterizedPass(const std::string& name) : GPUPass(name){};

    virtual ~GPURasterizedPass()
    {

    }

    virtual GPUPassType getType() const override
    {
        return GPUPassType::Graphics;
    }

    void renderTo(PassTextureDescription* texture, vk::AttachmentLoadOp loadOp = vk::AttachmentLoadOp::eDontCare)
    {
        PassResourceAccessInfo accessInfo;
        accessInfo.accessType = PassResourceAccess::RenderTarget{ loadOp,vk::AttachmentStoreOp::eStore };
        accessInfo.pass = this;
        texture->accessList.emplace_back(std::move(accessInfo));
        this->renderTargets.emplace_back(RenderTarget{loadOp, vk::AttachmentStoreOp::eStore, texture});
    }

    bool checkWrite(PassResourceDescriptionBase* res) override
    {
        return false;
    }

    bool checkRead(PassResourceDescriptionBase* res) override
    {
        return false;
    }

    bool fillMemBarrierInfo(PassResourceAccess::Access accessType, PassBufferDescription* buffer, vk::MemoryBarrier2& barrier) override
    {
        return false;
    }
    bool fillImgBarrierInfo(PassResourceAccess::Access accessType, PassTextureDescription* texture,
        vk::PipelineStageFlags2& stageMask, vk::AccessFlags2& accessMask, vk::ImageLayout& layout) override
    {
        if (std::holds_alternative<PassResourceAccess::Read>(accessType))
        {
            stageMask = vk::PipelineStageFlagBits2::eFragmentShader;
            accessMask = vk::AccessFlagBits2::eShaderRead;
            if (isDepthStencilFormat(texture->format))
            {
                layout = vk::ImageLayout::eDepthStencilReadOnlyOptimal;
            }
            else {
                layout = vk::ImageLayout::eShaderReadOnlyOptimal;
            }
            return true;
        }
        if (std::holds_alternative<PassResourceAccess::Write>(accessType))
        {
            stageMask = vk::PipelineStageFlagBits2::eFragmentShader;
            accessMask = vk::AccessFlagBits2::eShaderRead;
            layout = vk::ImageLayout::eGeneral;
            return true;
        }
        if (std::holds_alternative<PassResourceAccess::Sample>(accessType))
        {
            stageMask = vk::PipelineStageFlagBits2::eFragmentShader;
            accessMask = vk::AccessFlagBits2::eShaderSampledRead;
            if (isDepthStencilFormat(texture->format))
            {
                layout = vk::ImageLayout::eDepthStencilReadOnlyOptimal;
            }
            else {
                layout = vk::ImageLayout::eShaderReadOnlyOptimal;
            }
            return true;
        }
        if (std::holds_alternative<PassResourceAccess::RenderTarget>(accessType))
        {
            if (isDepthStencilFormat(texture->format))
            {
                stageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests;
                accessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite;
                layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
            }
            else {
                stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
                accessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
                layout = vk::ImageLayout::eColorAttachmentOptimal;
            }
            return true;
        }

        return false;
    }

    void record2(vk::CommandBuffer cmd, GPUFrame* frame) override;
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

    PassTextureDescription* createTexture(const std::string& name, vk::Format format, PassTextureExtentType extentType = PassTextureExtent::SwapchainRelative{})
    {
        auto tex = std::make_unique<PassTexture>(name, format, std::move(extentType));
        auto* tex_ptr = tex.get();
        this->textureDescriptions.emplace_back(std::move(tex));
        return tex_ptr;
    }

    PassTextureDescription* getSwapchainTexture()
    {
        return swapchainTextureDesc.get();
    }

    void executePass(std::unique_ptr<GPUPass>&& pass)
    {
        _allPasses.emplace_back(std::move(pass));
    }

    struct PassDataDescriptorSetBaseLayout {
        PassDataDescriptorSetBaseLayout()
        = default;

        std::vector<vk::DescriptorSetLayoutBinding> bindings;
        // Initial write is incomplete. DstSet need to be set.
        std::vector<vk::WriteDescriptorSet> writes;
        std::shared_ptr<vk::DescriptorImageInfo[]> imgInfos{};
        std::shared_ptr<vk::DescriptorBufferInfo[]> bufInfos{};
    };

    /*
     * Get base layout(include previous pass attachment as input texture).
     * Note that writeDescriptorSets cannot be directly used : caller must
     * manually set the dstSet.
     * */

    //https://app.diagrams.net/#G1gIpgDwpK7Vyhzypl7A_RbQFETWhS1_1q
    void compileAOT();

    void allocateResources();

    void buildBarriers();

    enum class Event
    {
        SWAPCHAIN_RESIZE
    };

    void update(Event event);

    void disablePass(const std::string& passName);

    void enablePass(const std::string& passName);

    auto createBackingImage(PassTextureDescription* textureDesc)
    {
        auto textureExtent = textureDesc->getExtent(backendDevice->_swapchain);
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = static_cast<VkFormat>(textureDesc->format);
        imageInfo.extent = VkExtent3D{ textureExtent.width,textureExtent.height,1};
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
            if (isDepthStencilFormat(textureDesc->format))
            {
                imageInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            }
            else {
                imageInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            }
        }
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        return backendDevice->allocateVMAImage(imageInfo);
    }

    auto createBackingImageView(PassTextureDescription* textureDesc, const PassResourceAccess::Access & accessType, vk::Image image)
    {
        vk::ImageViewCreateInfo imageViewInfo{};
        imageViewInfo.image = image;
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
        if (isDepthStencilFormat(textureDesc->format))
        {
            subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
        }
        else {
            subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        }
        if (std::holds_alternative<PassResourceAccess::Sample>(accessType))
        {
            subresourceRange.baseMipLevel = 0;
            subresourceRange.levelCount = 1;
        }
        else if (std::holds_alternative<PassResourceAccess::RenderTarget>(accessType))
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
        return backendDevice->createImageView(imageViewInfo);
    }

    vk::ImageView getBackingImageView(const std::string &name) const {
        return backingImageViews.find(name)->second;
    }

    vk::Image getBackingImage(const std::string& name) const {
        return backingImages.find(name)->second.image;
    }

    int allocateSingleDescriptorSet(vk::DescriptorSetLayout layout) const {
        return 0;
    }

    void getAllocatedDescriptorSet(int handle, const std::function<void(vk::DescriptorSet)> &callback) const {

    }

    GPUPass* getPass(const std::string & name)
    {
        for (auto & pass  : this->_allPasses)
        {
            if (pass->_name == name)
            {
                return pass.get();
            }
        }
        return nullptr;
    }

    using DescriptorSetRecord = std::pair<std::string,vk::DescriptorSet>;
    using DescriptorSetRecordList = std::vector<DescriptorSetRecord>;
    using DescriptorSetLayoutRecord = std::pair<DescriptorSetLayoutExtended,DescriptorSetRecordList>;
    using DescriptorSetCallback = std::function<void(const vk::DescriptorSet &)>;

    vk::DescriptorSetLayout manageDescriptorSet(std::string && name, const std::vector<vk::DescriptorSetLayoutBinding> & bindings);

    void getManagedDescriptorSet(std::string && setName, const std::function<void(vk::DescriptorSet)>& cb);

    void prepareDescriptorSetsAOT();

    vk::DescriptorSet getManagedDescriptorSet(std::string && name) const;

    void managePassInputDescriptorSet(GPUPass* pass);

    std::unique_ptr<PassTextureDescription> swapchainTextureDesc;
    std::vector<int> sortedIndices;
    std::vector<std::unique_ptr<GPUPass>> _allPasses;

    std::vector<std::unique_ptr<PassTexture>> textureDescriptions;
    std::vector<std::unique_ptr<PassBuffer>> bufferDescriptions;

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

    bool needToRebuildBarriers = false;
};

#endif //PBRTEDITOR_GPUPASS_H

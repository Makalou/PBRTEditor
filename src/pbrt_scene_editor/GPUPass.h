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
#include "FrameGraphResourceDef.hpp"

struct GPUFrame;
struct FrameCoordinator;

enum class GPUPassType
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

    std::vector<FrameGraphResourceDescriptionBase*> reads;
    std::vector<FrameGraphResourceDescriptionBase*> writes;
    std::vector<FrameGraphTextureDescription*> samples;

    vk::DescriptorSetLayout passInputDescriptorSetLayout;
    bool force_disabled = false;
    bool is_enabled() { return !force_disabled && enableCond(); }
    bool is_first_enabled = true;
    bool is_switched_to_enabled = true;
    std::function<bool(void)> enableCond = [] {return true; };
    /*
     * For some passes, the shaders, pipelines to use can be determined Ahead of Time,
     * And would never change during the whole applications time.
     * 
     * This method will usually be called only once when frame graph was first compiled.
     * Ideally one should do as many ahead of time jobs as possible. Such as build all the shaders and pipelines
     * that would be used by the pass, prepare other immutable per frame resources.
     */
    virtual void prepareAOT(FrameCoordinator* coordinator) = 0;

    /*
     * For some passes, it's hard or impossible to decide which shaders or pipeline to use ahead of time.
     * And maybe new shader variants can appear at every frame.
     * Also, per pass data can change every frame.
     * That's why we also provide functionality to update the pass on-the-fly.
     * */

    // Invoked when the pass is enabled on the first time
    virtual void onFirstEnable(GPUFrame* frame);

    // Invoked at every iteration if the pass is enabled
    virtual void onEnable(GPUFrame* frame);

    // Invoked once when the pass is switche to enable state
    virtual void onSwitchToEnable(GPUFrame* frame);

    void read(FrameGraphTextureDescription* texture);

    void write(FrameGraphTextureDescription* texture);

    void readwrite(FrameGraphTextureDescription* texture);

    void sample(FrameGraphTextureDescription* texture, bool mipmap = false);

    virtual bool fillMemBarrierInfo(FrameGraphResourceAccess::Access accessType, FrameGraphBufferDescription* buffer,vk::MemoryBarrier2& barrier) = 0;
    virtual bool fillImgBarrierInfo(FrameGraphResourceAccess::Access accessType, FrameGraphTextureDescription* texture,
        vk::PipelineStageFlags2& stageMask, vk::AccessFlags2& accessMask, vk::ImageLayout& layout) = 0;

    std::vector<PassActionContext> actionContextQueue;

    void virtual record(vk::CommandBuffer cmd, GPUFrame* frame) = 0;

    // We resolve the barriers for each frame on-the-fly to avoid many tricky situation
    std::vector<vk::MemoryBarrier2> transientMemoryBarriers;
    std::vector<vk::BufferMemoryBarrier2> transientBufferMemoryBarriers;
    std::vector<vk::ImageMemoryBarrier2> transientImageMemoryBarriers;

    void insertPipelineBarrier(vk::CommandBuffer cmd
#if __APPLE__
                               ,const vk::DispatchLoaderDynamic& d
#endif
                               )
    {
        if (transientMemoryBarriers.empty() && transientBufferMemoryBarriers.empty() && transientImageMemoryBarriers.empty())
        {
            return;
        }
        vk::DependencyInfo info{};
        info.setMemoryBarriers(transientMemoryBarriers);
        info.setBufferMemoryBarriers(transientBufferMemoryBarriers);
        info.setImageMemoryBarriers(transientImageMemoryBarriers);
#if __APPLE__
        cmd.pipelineBarrier2KHR(info,d);
#else
        cmd.pipelineBarrier2(info);
#endif
        transientMemoryBarriers.clear();
        transientBufferMemoryBarriers.clear();
        transientImageMemoryBarriers.clear();
    }
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

    bool fillMemBarrierInfo(FrameGraphResourceAccess::Access accessType, FrameGraphBufferDescription* buffer, vk::MemoryBarrier2& barrier) override;

    bool fillImgBarrierInfo(FrameGraphResourceAccess::Access accessType, FrameGraphTextureDescription* texture,
        vk::PipelineStageFlags2& stageMask, vk::AccessFlags2& accessMask, vk::ImageLayout& layout);

    void record(vk::CommandBuffer cmd, GPUFrame* frame) override;

    std::vector<VulkanComputePipeline> computePipelines;
};

struct RenderTarget
{
    vk::AttachmentLoadOp loadOp;
    vk::AttachmentStoreOp storeOp;
    FrameGraphTextureDescription* texture;
};

struct GPURasterizedPass : GPUPass
{
    void beginPass(vk::CommandBuffer cmdBuf,vk::Framebuffer framebuffer);

    void endPass(vk::CommandBuffer cmdBuf);

    void buildRenderPass(DeviceExtended* device);

    vk::Framebuffer buildFrameBuffer(GPUFrame* frame);
    vk::Framebuffer buildSwapChainTouchedFrameBuffer(DeviceExtended* device,int width, int height, vk::ImageView swapchainImageView);

    std::vector<RenderTarget> renderTargets;

    vk::RenderPass renderPass;
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

    ~GPURasterizedPass() override
    {

    }

    GPUPassType getType() const override
    {
        return GPUPassType::Graphics;
    }

    void renderTo(FrameGraphTextureDescription* texture, vk::AttachmentLoadOp loadOp = vk::AttachmentLoadOp::eDontCare);

    bool fillMemBarrierInfo(FrameGraphResourceAccess::Access accessType, FrameGraphBufferDescription* buffer, vk::MemoryBarrier2& barrier) override;

    bool fillImgBarrierInfo(FrameGraphResourceAccess::Access accessType, FrameGraphTextureDescription* texture,
        vk::PipelineStageFlags2& stageMask, vk::AccessFlags2& accessMask, vk::ImageLayout& layout) override;

    void record(vk::CommandBuffer cmd, GPUFrame* frame) override;
};

struct GPURayTracingPass : GPUPass
{
    std::vector<std::pair<vk::Pipeline,vk::RayTracingPipelineCreateInfoKHR>> computePipelines;
    std::vector<vk::ShaderModule> shaders;
};

#endif //PBRTEDITOR_GPUPASS_H

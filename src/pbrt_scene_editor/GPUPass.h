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

using GPUPassHandle = GPUPass*;
using PassResourceHandle = PassResourceDescriptionBase*;

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
    GPUPassHandle producer;
    //used to link an input to its output resource
    PassResourceHandle outputHandle;
    // uuid
    std::string name;

    virtual PassResourceType getType() const = 0;

    explicit PassResourceDescriptionBase(const std::string& nam) : name(nam)
    {

    }

    virtual ~PassResourceDescriptionBase(){

    };
};

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
    int width = 0;
    int height = 0;
    vk::AttachmentLoadOp loadOp{};
    vk::AttachmentStoreOp storeOp{};

    PassAttachmentDescription(const std::string& name, vk::Format format, int width, int height, vk::AttachmentLoadOp loadOp, vk::AttachmentStoreOp storeOp)
    : PassResourceDescriptionBase(name)
    {
        this->format = format;
        this->width = width;
        this->height = height;
        this->loadOp = loadOp;
        this->storeOp = storeOp;
    }

    PassAttachmentDescription(const std::string& name,vk::AttachmentLoadOp loadOp, vk::AttachmentStoreOp storeOp)
            : PassResourceDescriptionBase(name)
    {
        this->loadOp = loadOp;
        this->storeOp = storeOp;
    }

    PassResourceType getType() const override
    {
        return Attachment;
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

    GPUPass(const std::string& name) : _name(name)
    {

    }

    DeviceExtended* backend_device;
    std::vector<PassResourceDescriptionBase*> inputs;
    std::vector<PassResourceDescriptionBase*> outputs;
    std::vector<GPUPassHandle> edges;
    bool enabled = true;
    /*
     * For some passes, the shaders, pipelines to use can be determined Ahead of Time,
     * And would never change during the whole applications time.
     * This method will usually be called only once when frame graph was first compiled.
     * Ideally one should do as many ahead of time jobs as possible. Such as build all the shaders and pipelines
     * that would be used by the pass, prepare other immutable per pass resources.
     */
    virtual void compileAOT() = 0;

    /*
     * For some passes, it's hard or impossible to decide which shaders or pipeline to use ahead of time.
     * And maybe new shader variants can appear at every frame.
     * Also per pass data can change every frame, so this function also plays the role of 'Prepare'
     * */
    virtual void compileJIT(){};

    void addInput(std::unique_ptr<PassResourceDescriptionBase> resource)
    {
        inputs.push_back(resource.release());
    }

    void addOutput(std::unique_ptr<PassResourceDescriptionBase> resource)
    {
        outputs.push_back(resource.release());
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
    virtual void record(vk::CommandBuffer cmdBuf, int frameIdx) = 0;
    void beginPass(vk::CommandBuffer cmdBuf);

    void endPass(vk::CommandBuffer cmdBuf);

    void bindPassData(vk::CommandBuffer cmdBuf, int frameIdx)
    {
        //cmdBuf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,passBaseLayout,0,passDataDescriptorSet, nullptr);
    }

    void buildRenderPass(const DeviceExtended& device, GPUFrame* frame);

    void buildFrameBuffer(const DeviceExtended& device, GPUFrame* frame);

    vk::RenderPass renderPass;
    vk::Framebuffer frameBuffer;

    std::vector<std::pair<std::pair<const VertexShader*, const FragmentShader*>, VulkanGraphicsPipeline>> graphicsPipelines;
    vk::PipelineLayout passBaseLayout;
    vk::PipelineLayoutCreateInfo passBaseLayoutInfo{};
    vk::DescriptorSet passDataDescriptorSet;
    //We should guarantee that each pipelineLayouts must be compatible with passBaseLayout
    std::vector<std::pair<std::vector<vk::DescriptorSetLayout>, vk::PipelineLayout>> pipelineLayouts;

    GPURasterizedPass(const std::string& name) : GPUPass(name){};
};

struct TransferPass : GPUPass
{

};

struct GPURayTracingPass : GPUPass
{
    std::vector<std::pair<vk::Pipeline,vk::RayTracingPipelineCreateInfoKHR>> computePipelines;
    std::vector<vk::ShaderModule> shaders;
};

struct GPUFrame
{
    GPUFrame(int threadsNum, std::shared_ptr<DeviceExtended> backendDevice) :
    workThreadsNum(threadsNum),backendDevice(backendDevice)
    {
        uint32_t mainQueueFamilyIdx = backendDevice->get_queue_index(vkb::QueueType::graphics).value();
        vk::CommandPoolCreateInfo poolInfo{};
        poolInfo.queueFamilyIndex = mainQueueFamilyIdx;
        poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

        for (int i = 0; i < workThreadsNum; i++)
        {
            CommandPoolExtended pool(backendDevice,backendDevice->createCommandPool(poolInfo));
            perThreadMainCommandAllocators.emplace_back(pool);
        }

        vk::Format swapChainFormat = static_cast<vk::Format>(backendDevice->_swapchain.image_format);
        auto swapChainExtent = backendDevice->_swapchain.extent;
        swapchainAttachment = new PassAttachmentDescription("SwapchainImage",swapChainFormat,swapChainExtent.width,swapChainExtent.height,
                                                        vk::AttachmentLoadOp::eDontCare,vk::AttachmentStoreOp::eDontCare);

        //todo multiple queues stuffs ...
    }

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

    vk::CommandBuffer recordMainQueueCommands()
    {

        for(auto allocator : perThreadMainCommandAllocators)
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
         * in to the same queue, no matter whether there are recorded into same command buffer.
         *
         * All in all, the only two reason we use multiple command buffers is 1. we want to serve different type of
         * operation(which doesn't make too much different if we only use omnipotent main queue)
         * 2. we cannot operate on single command buffer in parallel.
         */
        int threadId = 0;
        auto cmdAllocator = perThreadMainCommandAllocators[threadId];
        auto cmdPrimary = cmdAllocator.getOrAllocateNextPrimary();

        for(int i = 0 ; i < _rasterPasses.size(); i ++)
        {
            vk::CommandBufferBeginInfo beginInfo{};
            beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse);
            cmdPrimary.begin(beginInfo);
            _rasterPasses[i]->record(cmdPrimary,frameIdx);
            cmdPrimary.end();
        }

        return cmdPrimary;
    }

    void registerRasterizedGPUPass(const std::shared_ptr<GPURasterizedPass> & pass)
    {
        for(const auto & reg_pass : _rasterPasses)
        {
            if(pass->_name == reg_pass->_name)
            {
                throw std::invalid_argument("Pass has been registered.");
            }
        }
        _rasterPasses.emplace_back(pass);
    }

    //https://app.diagrams.net/#G1gIpgDwpK7Vyhzypl7A_RbQFETWhS1_1q
    void compileAOT();

    vk::ImageView getBackingImageView(const std::string & name)
    {
        return backingImageViews[name];
    }

    PassAttachmentDescription* swapchainAttachment;
    std::vector<int> sortedIndices;
    std::vector<std::shared_ptr<GPURasterizedPass>> _rasterPasses;
    std::unordered_map<std::string,vk::ImageView> backingImageViews;
    std::unordered_map<std::string,VMAImage> backingImages;
    int frameIdx{};
};

#endif //PBRTEDITOR_GPUPASS_H

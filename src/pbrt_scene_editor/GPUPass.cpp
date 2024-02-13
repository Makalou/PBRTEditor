//
// Created by 王泽远 on 2024/1/18.
//

#include "GPUPass.h"
#include <stack>
#include "window.h"

bool isDepthStencilFormat(vk::Format format)
{
        return format == vk::Format::eD32Sfloat ||
        format == vk::Format::eD16Unorm ||
        format == vk::Format::eD16UnormS8Uint ||
        format == vk::Format::eD24UnormS8Uint ||
        format == vk::Format::eD32SfloatS8Uint;
};

vk::PipelineLayoutCreateInfo concatLayoutCreateInfo(vk::PipelineLayoutCreateInfo baseInfo, vk::PipelineLayoutCreateInfo derivedInfo)
{
    std::vector<vk::DescriptorSetLayout> setLayouts;
    setLayouts.reserve(baseInfo.setLayoutCount + derivedInfo.setLayoutCount);
    for(int i = 0; i < baseInfo.setLayoutCount; i ++)
    {
        setLayouts.push_back(baseInfo.pSetLayouts[i]);
    }
    for(int i = 0; i < baseInfo.setLayoutCount; i ++)
    {
        setLayouts.push_back(derivedInfo.pSetLayouts[i]);
    }

    vk::PipelineLayoutCreateInfo newInfo{};
    newInfo.setSetLayouts(setLayouts);
    return newInfo;
}

void GPURasterizedPass::beginPass(vk::CommandBuffer cmdBuf) {
    vk::RenderPassBeginInfo beginInfo{};
    beginInfo.setRenderPass(renderPass);
    beginInfo.setFramebuffer(frameBuffer);
    vk::Rect2D renderArea{};
    renderArea.offset.x = 0;
    renderArea.offset.y = 0;
    renderArea.extent.width = framebufferCreateInfo.width;
    renderArea.extent.height = framebufferCreateInfo.height;
    beginInfo.setRenderArea(renderArea);
    //todo hard-code for now
    beginInfo.setClearValueCount(1);
    auto clearValue = vk::ClearValue{};
    clearValue.setColor(vk::ClearColorValue{});
    beginInfo.setClearValues(clearValue);
    cmdBuf.beginRenderPass(beginInfo,vk::SubpassContents::eInline);
}

void GPURasterizedPass::endPass(vk::CommandBuffer cmdBuf) {
    cmdBuf.endRenderPass();
}

void GPURasterizedPass::buildRenderPass(const DeviceExtended &device, GPUFrame *frame) {

    attachmentDescriptions.clear();
    for(int i = 0;i<outputs.size();i++)
    {
        if(outputs[i]->getType() == Attachment)
        {
            vk::AttachmentDescription attachment{};
            auto * passAttachment = dynamic_cast<PassAttachmentDescription*>(outputs[i].get());
            attachment.setFormat(passAttachment->format);
            attachment.setSamples(vk::SampleCountFlagBits::e1);
            //todo how to deduce attachment layout and load/store OP?
            attachment.setInitialLayout(passAttachment->initialLayout);
            if(passAttachment->finalLayout == vk::ImageLayout::eUndefined)
            {
                if(isDepthStencilFormat(passAttachment->format))
                {
                    passAttachment->finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
                }else{
                    passAttachment->finalLayout = vk::ImageLayout::eColorAttachmentOptimal;
                }
            }
            attachment.setFinalLayout(passAttachment->finalLayout);
            attachment.setLoadOp(passAttachment->loadOp);
            attachment.setStoreOp(passAttachment->storeOp);
            attachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
            attachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
            attachmentDescriptions.emplace_back(attachment);
        }
    }

    renderPassCreateInfo.setAttachments(attachmentDescriptions);

    subpassInfo = vk::SubpassDescription{};
    subpassInfo.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);

    colorAttachmentRefs.clear();
    colorAttachmentRefs.reserve(attachmentDescriptions.size());
    depthStencilAttachmentRef = vk::AttachmentReference{};

    for(int i = 0; i < attachmentDescriptions.size(); i ++)
    {
        if(isDepthStencilFormat(attachmentDescriptions[i].format))
        {
            depthStencilAttachmentRef.attachment = i;
            //the layout the attachment uses during the subpass.
            depthStencilAttachmentRef.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
        }else{
            vk::AttachmentReference colorRef{};
            colorRef.attachment = i;
            colorRef.layout = vk::ImageLayout::eColorAttachmentOptimal;
            colorAttachmentRefs.emplace_back(colorRef);
        }
    }

    subpassInfo.setColorAttachments(colorAttachmentRefs);
    if(depthStencilAttachmentRef.layout != vk::ImageLayout::eUndefined)
    {
        subpassInfo.setPDepthStencilAttachment(&depthStencilAttachmentRef);
    }
    //subpass.setResolveAttachments();
    // We only use one subpass per render pass
    renderPassCreateInfo.setSubpasses(subpassInfo);
    // Since we only have one subpass, we don't care about the dependency ...
    // todo or do we?
    renderPassCreateInfo.setDependencies({});
    renderPass = device.createRenderPass(renderPassCreateInfo);
}

/*
 * May need recreate when swapChain resize
 */
void GPURasterizedPass::buildFrameBuffer(const DeviceExtended& device, GPUFrame* frame) {
    assert(renderPass!=VK_NULL_HANDLE);
    framebufferCreateInfo.setRenderPass(renderPass);
    //For now, we assume every attachment has same size
    int width = 0;
    int height = 0;

    std::vector<vk::ImageView> imageViews{};

    for(int i = 0; i < outputs.size();i++)
    {
        if(outputs[i]->getType() == Attachment)
        {
            auto passAttachment = dynamic_cast<PassAttachmentDescription*>(outputs[i].get());

            if ( width == 0 ) {
                width = passAttachment->width;
            } else {
                assert( width == passAttachment->width );
            }

            if ( height == 0 ) {
                height = passAttachment->height;
            } else {
                assert( height == passAttachment->height );
            }
            // The most interesting thing in frame buffer creation
            // is we need the frame graph to provide the underlying
            // memory of each attachment.
            //auto imgViewIdentifier = (outputs[i]->name == "SwapChainImage") ? "SwapChainImage" : _name+"::"+outputs[i]->name;
            auto imgViewIdentifier = _name+"::"+outputs[i]->name;
            imageViews.push_back(frame->getBackingImageView(imgViewIdentifier));
        }
    }

    framebufferCreateInfo.setWidth(width);
    framebufferCreateInfo.setHeight(height);
    framebufferCreateInfo.setLayers(1);
    framebufferCreateInfo.setAttachments(imageViews);
    frameBuffer = device.createFramebuffer(framebufferCreateInfo);
}

bool isResourceTypeCompatible(PassResourceType outputType, PassResourceType inputType)
{
    return outputType == inputType ||
            (outputType == Attachment && inputType == Texture);
}

/*
 * Deduce information for input and output
 */
void connectResources(PassResourceDescriptionBase* output, PassResourceDescriptionBase* input)
{
    if(!isResourceTypeCompatible(output->getType(),input->getType()))
    {
        throw std::invalid_argument("output and input type not compatible");
    }
    if(output->getType() == Attachment)
    {
        auto outputAttachment = dynamic_cast<PassAttachmentDescription*>(output);
        if(input->getType() == Attachment)
        {
            auto inputAttachment = dynamic_cast<PassAttachmentDescription*>(input);
            if(inputAttachment->format == vk::Format::eUndefined)
            {
                inputAttachment->format = outputAttachment->format;
            }
            if(inputAttachment->width == 0)
            {
                inputAttachment->width = outputAttachment->width;
            }
            if(inputAttachment->height == 0)
            {
                inputAttachment->height = outputAttachment->height;
            }

            if(isDepthStencilFormat(inputAttachment->format))
            {
                outputAttachment->finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
                inputAttachment->initialLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
            }else{
                outputAttachment->finalLayout = vk::ImageLayout::eColorAttachmentOptimal;
                inputAttachment->initialLayout = vk::ImageLayout::eColorAttachmentOptimal;
            }
        }else{
            auto inputTexture = dynamic_cast<PassTextureDescription*>(input);
            if(inputTexture->format == vk::Format::eUndefined)
            {
                inputTexture->format = outputAttachment->format;
            }
            if(inputTexture->width == 0)
            {
                inputTexture->width = outputAttachment->width;
            }
            if(inputTexture->height == 0)
            {
                inputTexture->height = outputAttachment->height;
            }

            if(isDepthStencilFormat(outputAttachment->format))
            {
                outputAttachment->finalLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal;
            }else{
                outputAttachment->finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            }
        }
    }else if(output->getType() == Texture)
    {
        auto outputTexture = dynamic_cast<PassAttachmentDescription*>(output);
        auto inputTexture = dynamic_cast<PassAttachmentDescription*>(output);

        if(inputTexture->format == vk::Format::eUndefined)
        {
            inputTexture->format = outputTexture->format;
        }
        if(inputTexture->width == 0)
        {
            inputTexture->width = outputTexture->width;
        }
        if(inputTexture->height == 0)
        {
            inputTexture->height = outputTexture->height;
        }

    }else{
        auto outputBuffer = dynamic_cast<PassAttachmentDescription*>(output);
        auto inputBuffer = dynamic_cast<PassAttachmentDescription*>(output);
    }
}

GPUFrame::GPUFrame(int threadsNum, const std::shared_ptr<DeviceExtended> &backendDevice) : workThreadsNum(threadsNum),backendDevice(backendDevice)
{
    uint32_t mainQueueFamilyIdx = backendDevice->get_queue_index(vkb::QueueType::graphics).value();
    vk::CommandPoolCreateInfo commandPoolInfo{};
    commandPoolInfo.queueFamilyIndex = mainQueueFamilyIdx;
    commandPoolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

    for (int i = 0; i < workThreadsNum; i++)
    {
        CommandPoolExtended pool(backendDevice,backendDevice->createCommandPool(commandPoolInfo));
        perThreadMainCommandAllocators.emplace_back(pool);
    }

    auto swapChainFormat = static_cast<vk::Format>(backendDevice->_swapchain.image_format);
    auto swapChainExtent = backendDevice->_swapchain.extent;
    swapchainAttachment = new PassAttachmentDescription("SwapchainImage",swapChainFormat,swapChainExtent.width,swapChainExtent.height,
                                                        vk::AttachmentLoadOp::eDontCare,vk::AttachmentStoreOp::eDontCare);

    //todo multiple queues stuffs ...
    std::array<vk::DescriptorSetLayoutBinding,1> bindings;

    for(auto & binding : bindings)
    {
        binding.setBinding(0);
        binding.setStageFlags(vk::ShaderStageFlagBits::eAll);
        binding.setDescriptorType(vk::DescriptorType::eUniformBuffer);
        binding.setDescriptorCount(1);
    }

    vk::DescriptorSetLayoutCreateInfo layoutCreateInfo;
    layoutCreateInfo.setBindings(bindings);
    _frameGlobalDescriptorSetLayout = backendDevice->createDescriptorSetLayout(layoutCreateInfo);

    vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.setSetLayouts(_frameGlobalDescriptorSetLayout);
    _frameLevelPipelineLayout = backendDevice->createPipelineLayout(pipelineLayoutCreateInfo);

    vk::DescriptorPoolCreateInfo descriptorPoolInfo{};
    descriptorPoolInfo.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);
    descriptorPoolInfo.setMaxSets(100);
    std::array<vk::DescriptorPoolSize,1> poolSizes;
    for(auto & poolSz : poolSizes)
    {
        poolSz.setType(vk::DescriptorType::eUniformBuffer);
        poolSz.setDescriptorCount(1);
    }
    descriptorPoolInfo.setPoolSizes(poolSizes);
    _frameGlobalDescriptorSetPool = backendDevice->createDescriptorPool(descriptorPoolInfo);
    vk::DescriptorSetAllocateInfo allocateInfo{};
    allocateInfo.setSetLayouts(_frameGlobalDescriptorSetLayout);
    allocateInfo.setDescriptorPool(_frameGlobalDescriptorSetPool);
    allocateInfo.setDescriptorSetCount(1);
    _frameGlobalDescriptorSet = backendDevice->allocateDescriptorSets(allocateInfo)[0];
    auto buf = backendDevice->allocateObservedBufferPull<frameGlobalData>(VkBufferUsageFlagBits::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT).value();
    _frameGlobalDataBuffer = buf;

    //Resetting a descriptor pool recycles all of the resources from all of the descriptor sets
    // allocated from the descriptor pool back to the descriptor pool, and the descriptor sets are implicitly freed.

    //Once all pending uses have completed, it is legal to update and reuse a descriptor set.
    //https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkCmdBindDescriptorSets.html
    vk::WriteDescriptorSet write{};
    write.setDescriptorType(vk::DescriptorType::eUniformBuffer);
    write.setDescriptorCount(1);
    write.setDstBinding(0);
    write.setDstSet(_frameGlobalDescriptorSet);
    vk::DescriptorBufferInfo bufferInfo;
    bufferInfo.setOffset(0);
    bufferInfo.setBuffer(_frameGlobalDataBuffer.getBuffer());
    bufferInfo.setRange(vk::WholeSize);
    write.setBufferInfo(bufferInfo);
    backendDevice->updateDescriptorSets(write,{});
}

void GPUFrame::compileAOT() {
    // And pass dependencies
    // After this process we guarantee that every input
    // has a valid producer.
    // And we only allow concurrent read to the same resource.
    for(int i = 0; i < _rasterPasses.size();i++)
    {
        auto & pass = _rasterPasses[i];
        for(int j = 0; j < pass->inputs.size(); j++)
        {
            auto inputName = pass->inputs[j]->name;
            if(inputName == "SwapchainImage")
            {
                pass->inputs[j]->outputHandle = swapchainAttachment;
                continue;
            }
            size_t pos = inputName.find("::");
            if(pos == std::string::npos)
                throw std::invalid_argument("Input resource must using PassName as name space");

            auto producerPassName = inputName.substr(0, pos);
            auto resourceName = inputName.substr(pos+2);

            GPUPass* producePass = nullptr;
            for(int k = 0; k < _rasterPasses.size();k++)
            {
                if(_rasterPasses[k]->_name == producerPassName)
                {
                    producePass = _rasterPasses[k].get();
                    break;
                }
            }

            if(producePass == nullptr)
            {
                throw std::invalid_argument("Can not find pass : " + producerPassName);
            }

            PassResourceDescriptionBase* output = nullptr;
            for(int k = 0; k < producePass->outputs.size();k++)
            {
                //todo : check concurrent usage to avoid RW or WW
                if(producePass->outputs[k]->name == resourceName)
                {
                    output = producePass->outputs[k].get();
                    pass->inputs[j]->outputHandle = output;
                    break;
                }
            }

            if(output == nullptr)
            {
                throw std::invalid_argument(producerPassName.append(" doesn't have output ").append(resourceName));
            }

            producePass->edges.push_back(pass.get());
        }
    }

    // Topological sorting
    std::stack<int> pass_idx_stk;
    std::vector<bool> visitedMasks(_rasterPasses.size());
    std::unordered_set<int> visited;
    sortedIndices.reserve(_rasterPasses.size());

    auto getPassIdx = [&](const std::string& name) ->int
    {
        for(int i = 0; i < _rasterPasses.size();i++)
        {
            if(_rasterPasses[i]->_name == name)
                return i;
        }
        return -1;
    };

    for(int i = 0; i < _rasterPasses.size(); i ++)
    {
        if(!visitedMasks[i])
        {
            pass_idx_stk.push(i);
            while(!pass_idx_stk.empty()){
                auto current = pass_idx_stk.top();
                bool allChildrenVisited = true;

                for(auto child : _rasterPasses[current]->edges)
                {
                    int child_idx = getPassIdx(child->_name);
                    if(!visitedMasks[child_idx]){
                        pass_idx_stk.push(child_idx);
                        allChildrenVisited = false;
                    }
                }

                if(allChildrenVisited){
                    pass_idx_stk.pop();
                    if(visited.find(current) == visited.end())
                    {
                        sortedIndices.push_back(current);
                        visited.insert(current);
                    }
                    visitedMasks[current] = true;
                }
            }
        }
    }

    std::reverse(sortedIndices.begin(), sortedIndices.end());

    backingImageViews.emplace("SwapchainImage",backendDevice->_swapchain.get_image_views().value()[frameIdx]);

    //create default sampler
    vk::SamplerCreateInfo defaultSamplerInfo{};
    defaultSamplerInfo.setAddressModeU(vk::SamplerAddressMode::eClampToBorder);
    defaultSamplerInfo.setAddressModeV(vk::SamplerAddressMode::eClampToBorder);
    defaultSamplerInfo.setAddressModeW(vk::SamplerAddressMode::eClampToBorder);
    defaultSamplerInfo.setAnisotropyEnable(vk::False);
    defaultSamplerInfo.setMaxAnisotropy(0.0);
    defaultSamplerInfo.setBorderColor(vk::BorderColor::eFloatOpaqueBlack);
    defaultSamplerInfo.setCompareEnable(vk::False);
    defaultSamplerInfo.setCompareOp(vk::CompareOp::eNever);
    defaultSamplerInfo.setMagFilter(vk::Filter::eLinear);
    defaultSamplerInfo.setMinFilter(vk::Filter::eLinear);
    defaultSamplerInfo.setMaxLod(0);
    defaultSamplerInfo.setMinLod(0);
    defaultSamplerInfo.setMipLodBias(0);
    defaultSamplerInfo.setMipmapMode(vk::SamplerMipmapMode::eLinear);

    samplers.emplace_back(backendDevice->createSampler(defaultSamplerInfo));

    for(int passIdx = 0; passIdx < sortedIndices.size(); passIdx++)
    {
        auto & pass = _rasterPasses[sortedIndices[passIdx]];
        for(int i = 0; i < pass->inputs.size();i++)
        {
            if(pass->inputs[i]->outputHandle)
                connectResources(pass->inputs[i]->outputHandle.pointer,pass->inputs[i].get());
        }
        int kk = 0;
        for(int i = 0; i < pass->outputs.size(); i++)
        {
            if(pass->outputs[i]->getType() == Attachment)
            {
                auto * outputi = dynamic_cast<PassAttachmentDescription*>(pass->outputs[i].get());
                auto name = pass->_name + "::" + outputi->name;
                bool match = false;
                for(;kk<pass->inputs.size(); kk++)
                {
                    if(pass->inputs[kk]->getType() == Attachment)
                    {
                        match = true;
                        auto * inputkk = dynamic_cast<PassAttachmentDescription*>(pass->inputs[kk].get());
                        //deduce information
                        if(outputi->format == vk::Format::eUndefined)
                        {
                            outputi->format = inputkk->format;
                        }
                        if(outputi->width == 0)
                        {
                            outputi->width = inputkk->width;
                        }
                        if(outputi->height == 0)
                        {
                            outputi->height = inputkk->height;
                        }
                        backingImageViews.emplace(name,backingImageViews[inputkk->name]);
                        break;
                    }
                }

                if(!match)
                {
                    // Need to create new image
                    auto attachmentDesc = dynamic_cast<PassAttachmentDescription*>(pass->outputs[i].get());
                    VMAImage image;
                    /*todo frame graph should some how determine whether an attachment need to create with sample bit or not.
                     * For color attachment, it seems that every attachment need to be sampled by further passes, except swapchainimage
                     * For depth attachment, it might not be the case.*/
                    bool sampled_need = true;
                    if(isDepthStencilFormat(attachmentDesc->format))
                    {
                        image = backendDevice->allocateVMAImageForDepthStencilAttachment(
                                static_cast<VkFormat>(attachmentDesc->format), attachmentDesc->width, attachmentDesc->height,sampled_need).value();
                    }else{
                        image = backendDevice->allocateVMAImageForColorAttachment(
                                static_cast<VkFormat>(attachmentDesc->format), attachmentDesc->width, attachmentDesc->height,sampled_need).value();
                    }

                    backingImages.emplace(name,image);
                    // create image view
                    vk::ImageViewCreateInfo imageViewInfo{};
                    imageViewInfo.image = image.image;
                    imageViewInfo.viewType = vk::ImageViewType::e2D;
                    imageViewInfo.format = attachmentDesc->format;
                    vk::ComponentMapping componentMapping{};
                    componentMapping.a = vk::ComponentSwizzle::eA;
                    componentMapping.r = vk::ComponentSwizzle::eR;
                    componentMapping.g = vk::ComponentSwizzle::eG;
                    componentMapping.b = vk::ComponentSwizzle::eB;
                    imageViewInfo.components = componentMapping;
                    vk::ImageSubresourceRange subresourceRange;
                    if(isDepthStencilFormat(attachmentDesc->format))
                    {
                        subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
                    }else{
                        subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
                    }
                    subresourceRange.baseMipLevel = 0;
                    subresourceRange.levelCount = 1;
                    subresourceRange.baseArrayLayer = 0;
                    subresourceRange.layerCount = 1;
                    imageViewInfo.subresourceRange = subresourceRange;
                    auto imageView = backendDevice->createImageView(imageViewInfo);
                    backingImageViews.emplace(name,imageView);
                }
            }
        }
    }

    // Build RenderPass
    for(auto & pass : _rasterPasses)
    {
        // In theory, if multiple frame in flight is allowed,
        // We need to build frame buffers for each gpu frame,
        // but we only need to build render pass once.
        pass->buildRenderPass(*backendDevice,this);
        pass->buildFrameBuffer(*backendDevice,this);
    }

    // Pass ahead of time preparation
    for(auto & pass : _rasterPasses)
    {
        pass->prepareAOT(this);
    }

    Window::registerCursorPosCallback([this](double xPos, double yPos){
        this->x = xPos;
        this->y = yPos;
    });
}

vk::CommandBuffer GPUFrame::recordMainQueueCommands() {
    _frameGlobalDataBuffer->time.x = static_cast<float>(glfwGetTime());
    _frameGlobalDataBuffer->cursorPos.x = this->x;
    _frameGlobalDataBuffer->cursorPos.y = this->y;

    //Resetting a descriptor pool recycles all of the resources from all of the descriptor sets
    // allocated from the descriptor pool back to the descriptor pool, and the descriptor sets are implicitly freed.

    //Once all pending uses have completed, it is legal to update and reuse a descriptor set.
    //https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkCmdBindDescriptorSets.html
    vk::WriteDescriptorSet write{};
    write.setDescriptorType(vk::DescriptorType::eUniformBuffer);
    write.setDescriptorCount(1);
    write.setDstBinding(0);
    write.setDstSet(_frameGlobalDescriptorSet);
    vk::DescriptorBufferInfo bufferInfo;
    bufferInfo.setOffset(0);
    bufferInfo.setBuffer(_frameGlobalDataBuffer.getBuffer());
    bufferInfo.setRange(vk::WholeSize);
    write.setBufferInfo(bufferInfo);

    backendDevice->updateDescriptorSets(write,{});

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

    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse);
    cmdPrimary.begin(beginInfo);
    vk::DebugUtilsLabelEXT frameLabel{};
    auto frameLabelName = std::string("Frame ").append(std::to_string(frameIdx));
    frameLabel.setPLabelName(frameLabelName.c_str());
    cmdPrimary.beginDebugUtilsLabelEXT(frameLabel,backendDevice->getDLD());
    cmdPrimary.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,_frameLevelPipelineLayout,0,_frameGlobalDescriptorSet,nullptr);
    for(int i = 0 ; i < _rasterPasses.size(); i ++)
    {
        _rasterPasses[i]->prepareIncremental(this);
        vk::DebugUtilsLabelEXT passLabel{};
        passLabel.setPLabelName(_rasterPasses[i]->_name.c_str());
        cmdPrimary.beginDebugUtilsLabelEXT(passLabel,backendDevice->getDLD());
        _rasterPasses[i]->record(cmdPrimary,this);
        cmdPrimary.endDebugUtilsLabelEXT(backendDevice->getDLD());
    }
    cmdPrimary.end();
    cmdPrimary.endDebugUtilsLabelEXT(backendDevice->getDLD());
    return cmdPrimary;
}

GPUFrame::PassDataDescriptorSetBaseLayout GPUFrame::getPassDataDescriptorSetBaseLayout(GPUPass *pass) const
{
    PassDataDescriptorSetBaseLayout baseLayout{};
    int bindingCount = 0;
    int imgInfoCount = 0;
    int bufInfoCount = 0;

    for(auto & input : pass->inputs)
    {
        if(input->getType() == PassResourceType::Texture || input->getType() == PassResourceType::Buffer) {
            bindingCount ++;
            if(input->getType() == PassResourceType::Texture)
            {
                imgInfoCount ++;
            }
            if(input->getType() == PassResourceType::Buffer)
            {
                bufInfoCount ++;
            }
        }
    }

    baseLayout.bindings.reserve(bindingCount);
    baseLayout.writes.reserve(bindingCount);

    if(imgInfoCount > 0)
    {
        std::unique_ptr<vk::DescriptorImageInfo[]> s(new vk::DescriptorImageInfo[imgInfoCount]);
        baseLayout.imgInfos.swap(s);
    }
    if(bufInfoCount > 0)
    {
        std::unique_ptr<vk::DescriptorBufferInfo[]> s(new vk::DescriptorBufferInfo[bufInfoCount]);
        baseLayout.bufInfos.swap(s);
    }

    int imgInfoBackIdx = 0;
    int bufInfoBackIdx = 0;

    for(auto & input : pass->inputs)
    {
        if(input->getType() == PassResourceType::Texture || input->getType() == PassResourceType::Buffer)
        {
            vk::DescriptorSetLayoutBinding binding;
            binding.setBinding(baseLayout.bindings.size());
            binding.setDescriptorCount(1);
            binding.setStageFlags(vk::ShaderStageFlagBits::eAllGraphics);
            vk::WriteDescriptorSet write{};
            write.setDescriptorCount(binding.descriptorCount);
            write.setDstBinding(binding.binding);

            if(input->getType() == PassResourceType::Texture)
            {
                binding.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
                write.setDescriptorType(binding.descriptorType);
                vk::DescriptorImageInfo imgInfo{};
                imgInfo.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
                imgInfo.setSampler(samplers[0]); // use default sampler
                auto imgView = getBackingImageView(input->name);
                imgInfo.setImageView(imgView);
                baseLayout.imgInfos[imgInfoBackIdx] = imgInfo;
                write.setPImageInfo(&baseLayout.imgInfos[imgInfoBackIdx]);
                imgInfoBackIdx ++;
            }
            if(input->getType() == PassResourceType::Buffer)
            {
                binding.setDescriptorType(vk::DescriptorType::eStorageBuffer);
                write.setDescriptorType(binding.descriptorType);
                //todo
            }
            baseLayout.bindings.push_back(binding);
            baseLayout.writes.push_back(write);
        }
    }

    return std::move(baseLayout);
}
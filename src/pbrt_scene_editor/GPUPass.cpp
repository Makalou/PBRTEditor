//
// Created by 王泽远 on 2024/1/18.
//

#include "GPUPass.h"
#include <stack>

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
    cmdBuf.beginRenderPass(beginInfo,vk::SubpassContents::eInline);
}

void GPURasterizedPass::endPass(vk::CommandBuffer cmdBuf) {
    cmdBuf.endRenderPass();
}

void GPURasterizedPass::buildRenderPass(const DeviceExtended &device, GPUFrame *frame) {
    vk::RenderPassCreateInfo renderPassCreateInfo{};
    std::vector<vk::AttachmentDescription> attachmentDescriptions;

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
            attachment.setFinalLayout(passAttachment->finalLayout);
            attachment.setLoadOp(passAttachment->loadOp);
            attachment.setStoreOp(passAttachment->storeOp);
            attachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
            attachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
            attachmentDescriptions.emplace_back(attachment);
        }
    }

    renderPassCreateInfo.setAttachments(attachmentDescriptions);

    vk::SubpassDescription subpassInfo{};
    subpassInfo.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
    std::vector<vk::AttachmentReference> colorAttachmentRefs;
    colorAttachmentRefs.reserve(attachmentDescriptions.size());
    vk::AttachmentReference depthStencilAttachmentRef;

    for(int i = 0; i < attachmentDescriptions.size(); i ++)
    {
        if(isDepthStencilFormat(attachmentDescriptions[i].format))
        {
            depthStencilAttachmentRef.attachment = i;
            //the layout the attachment uses during the subpass.
            depthStencilAttachmentRef.layout = attachmentDescriptions[i].initialLayout;
        }else{
            vk::AttachmentReference colorRef{};
            colorRef.attachment = i;
            colorRef.layout = attachmentDescriptions[i].initialLayout;
            colorAttachmentRefs.emplace_back(colorRef);
        }
    }

    subpassInfo.setColorAttachments(colorAttachmentRefs);
    subpassInfo.setPDepthStencilAttachment(&depthStencilAttachmentRef);
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

    for(int passIdx = 0; passIdx < sortedIndices.size(); passIdx++)
    {
        auto & pass = _rasterPasses[sortedIndices[passIdx]];
        for(int i = 0; i < pass->inputs.size();i++)
        {
            if(pass->inputs[i]->outputHandle)
                connectResources(pass->inputs[i]->outputHandle,pass->inputs[i].get());
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
                    if(isDepthStencilFormat(attachmentDesc->format))
                    {
                        image = backendDevice->allocateVMAImageForDepthStencilAttachment(
                                static_cast<VkFormat>(attachmentDesc->format), attachmentDesc->width, attachmentDesc->height).value();
                    }else{
                        image = backendDevice->allocateVMAImageForColorAttachment(
                                static_cast<VkFormat>(attachmentDesc->format), attachmentDesc->width, attachmentDesc->height).value();
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
}
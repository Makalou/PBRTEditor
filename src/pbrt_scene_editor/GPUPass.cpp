//
// Created by 王泽远 on 2024/1/18.
//

#include "GPUPass.h"

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
            auto passAttachment = dynamic_cast<PassAttachmentDescription*>(outputs[i]);
            attachment.setFormat(passAttachment->format);
            attachment.setSamples(vk::SampleCountFlagBits::e1);
            attachment.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal);
            attachment.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal);
            attachment.setLoadOp(passAttachment->loadOp);
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
    // Since we only have one subpass, we don't care about the dependency ... or do we?
    renderPassCreateInfo.setDependencies({});
    renderPass = device.createRenderPass(renderPassCreateInfo);
}

/*
 * May need recreate when swapChain resize
 */
void GPURasterizedPass::buildFrameBuffer(const DeviceExtended& device, GPUFrame* frame) {
    vk::FramebufferCreateInfo framebufferCreateInfo{};
    framebufferCreateInfo.setRenderPass(renderPass);
    //For now, we assume every attachment has same size
    int width = 0;
    int height = 0;

    std::vector<vk::ImageView> imageViews{};

    for(int i = 0; i < outputs.size();i++)
    {
        if(outputs[i]->getType() == Buffer)
        {
            continue;
        }

        auto passAttachment = dynamic_cast<PassAttachmentDescription*>(outputs[i]);

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

        if(passAttachment->format == vk::Format::eD32Sfloat)
        {

        }else{

        }
    }

    framebufferCreateInfo.setWidth(width);
    framebufferCreateInfo.setHeight(height);
    framebufferCreateInfo.setLayers(1);
    // The most interesting thing in frame buffer creation
    // is we need the frame graph to provide the underlying
    // memory of each attachment.
    framebufferCreateInfo.setAttachments(imageViews);
    frameBuffer = device.createFramebuffer(framebufferCreateInfo);
}

bool isResourceTypeCompatible(PassResourceType outputType, PassResourceType inputType)
{
    return outputType == inputType ||
            (outputType == Attachment && inputType == Texture);
}

/*
 * Deduce information for input
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
            if(inputName == "SwapchainImage") continue;
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
                //todo : check concurrent usage
                if(producePass->outputs[k]->name == resourceName)
                {
                    output = producePass->outputs[k];
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

    // Allocate real Image and ImageView
    backingImageViews.emplace("SwapchainImage",backendDevice->_swapchain.get_image_views().value()[frameIdx]);

    for(auto & pass : _rasterPasses)
    {
        int kk = 0;
        for(int i = 0; i < pass->outputs.size(); i++)
        {
            if(pass->outputs[i]->getType() == Attachment)
            {
                auto name = pass->_name + "::" + pass->outputs[i]->name;
                bool match = false;
                for(;kk<pass->inputs.size(); kk++)
                {
                    if(pass->inputs[kk]->getType() == Attachment)
                    {
                        match = true;
                        backingImageViews.emplace(name,backingImageViews[pass->inputs[kk]->name]);
                        break;
                    }
                }

                if(!match)
                {
                    // Need to create new image
                    auto attachmentDesc = dynamic_cast<PassAttachmentDescription*>(pass->outputs[i]);
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
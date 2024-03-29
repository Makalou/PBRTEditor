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
    clearValue.depthStencil.depth = 1.0f;
    beginInfo.setClearValues(clearValue);
    cmdBuf.beginRenderPass(beginInfo,vk::SubpassContents::eInline);
}

void GPURasterizedPass::endPass(vk::CommandBuffer cmdBuf) {
    cmdBuf.endRenderPass();
}

void GPURasterizedPass::buildRenderPass(const DeviceExtended &device, GPUFrame *frame) {

    attachmentDescriptions.clear();
    std::vector<PassAttachmentDescription*> passAttachmentDescriptions;
    passAttachmentDescriptions.reserve(outputs.size() + inouts.size());
    for(auto & output : outputs)
    {
        if(output->getType() == Attachment)
        {
            passAttachmentDescriptions.push_back(dynamic_cast<PassAttachmentDescription*>(output.get()));
        }
    }

    for (auto& inout : inouts)
    {
        if (inout->getType() == Attachment)
        {
            passAttachmentDescriptions.push_back(dynamic_cast<PassAttachmentDescription*>(inout.get()));
        }
    }
    
    for (PassAttachmentDescription* passAttachment : passAttachmentDescriptions)
    {
        vk::AttachmentDescription attachment{};
        attachment.setFormat(passAttachment->format);
        attachment.setSamples(vk::SampleCountFlagBits::e1);
        //todo how to deduce attachment layout and load/store OP?
        attachment.setInitialLayout(passAttachment->initialLayout);
        if (passAttachment->finalLayout == vk::ImageLayout::eUndefined)
        {
            if (isDepthStencilFormat(passAttachment->format))
            {
                passAttachment->finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
            }
            else {
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
    auto passName = this->_name + "RenderPass";
    device.setObjectDebugName(renderPass, passName.c_str());
}

/*
 * May need recreate when swapChain invalid
 */
void GPURasterizedPass::buildFrameBuffer(GPUFrame* frame) {
    assert(renderPass!=VK_NULL_HANDLE);
    framebufferCreateInfo.setRenderPass(renderPass);
    //For now, we assume each attachment has same size
    int width = 0;
    int height = 0;

    std::vector<vk::ImageView> imageViews{};

    std::vector<PassAttachmentDescription*> passAttachmentDescriptions;
    passAttachmentDescriptions.reserve(outputs.size() + inouts.size());

    for (auto& output : outputs)
    {
        if (output->getType() == Attachment)
        {
            auto passAttachment = dynamic_cast<PassAttachmentDescription*>(output.get());
            auto passAttachmentExtent = passAttachment->getExtent(frame->backendDevice->_swapchain);
            if (width == 0) {
                width = passAttachmentExtent.width;
            }
            else {
                assert(width == passAttachmentExtent.width);
            }

            if (height == 0) {
                height = passAttachmentExtent.height;
            }
            else {
                assert(height == passAttachmentExtent.height);
            }
            // The most interesting thing in frame buffer creation
            // is we need the frame graph to provide the underlying
            // memory of each attachment.
            //auto imgViewIdentifier = (outputs[i]->name == "SwapChainImage") ? "SwapChainImage" : _name+"::"+outputs[i]->name;
            auto imgViewIdentifier = _name + "::" + output->name;
            imageViews.push_back(frame->getBackingImageView(imgViewIdentifier));
        }
    }

    for (auto& inout : inouts)
    {
        if (inout->getType() == Attachment)
        {
            auto passAttachment = dynamic_cast<PassAttachmentDescription*>(inout.get());
            auto passAttachmentExtent = passAttachment->getExtent(frame->backendDevice->_swapchain);
            if (width == 0) {
                width = passAttachmentExtent.width;
            }
            else {
                assert(width == passAttachmentExtent.width);
            }

            if (height == 0) {
                height = passAttachmentExtent.height;
            }
            else {
                assert(height == passAttachmentExtent.height);
            }
            // The most interesting thing in frame buffer creation
            // is we need the frame graph to provide the underlying
            // memory of each attachment.
            //auto imgViewIdentifier = (outputs[i]->name == "SwapChainImage") ? "SwapChainImage" : _name+"::"+outputs[i]->name;
            size_t pos = inout->name.find("->");
            auto imgViewIdentifier = inout->name.substr(0, pos);
            imageViews.push_back(frame->getBackingImageView(imgViewIdentifier));
        }
    }

    framebufferCreateInfo.setWidth(width);
    framebufferCreateInfo.setHeight(height);
    framebufferCreateInfo.setLayers(1);
    framebufferCreateInfo.setAttachments(imageViews);
    frameBuffer = frame->backendDevice->createFramebuffer(framebufferCreateInfo);
    auto fbName = this->_name + "FrameBuffer";
    frame->backendDevice->setObjectDebugName(frameBuffer, fbName.c_str());
}

bool isResourceTypeCompatible(PassResourceType outputType, PassResourceType inputType)
{
    return outputType == inputType ||
            (outputType == Attachment && inputType == Texture);
}

/*
 * Deduce information for input and output
 */
void GPUFrame::connectResources(PassResourceDescriptionBase* output, PassResourceDescriptionBase* input)
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
            auto inputAttachmentExtent = inputAttachment->getExtent(backendDevice->_swapchain);
            if(inputAttachment->format == vk::Format::eUndefined)
            {
                inputAttachment->format = outputAttachment->format;
            }
            if (inputAttachmentExtent.width == 0 || inputAttachmentExtent.height == 0)
            {
                inputAttachment->extent = outputAttachment->extent;
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
            auto outputAttachmentExtent = outputAttachment->getExtent(backendDevice->_swapchain);
            if(inputTexture->width == 0 || inputTexture->height == 0)
            {
                inputTexture->width = outputAttachmentExtent.width;
                inputTexture->height = outputAttachmentExtent.height;
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
        auto outputTexture = dynamic_cast<PassTextureDescription*>(output);
        auto inputTexture = dynamic_cast<PassTextureDescription*>(output);

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
    swapchainAttachment = new PassAttachmentDescription("SwapchainImage",swapChainFormat,PassAttachmentExtent::SwapchainRelative(1,1),
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
    backendDevice->setObjectDebugName(_frameGlobalDescriptorSetLayout,"FrameGlobalDesciptorSetLayout");

    vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.setSetLayouts(_frameGlobalDescriptorSetLayout);
    _frameLevelPipelineLayout = backendDevice->createPipelineLayout(pipelineLayoutCreateInfo);
    backendDevice->setObjectDebugName(_frameLevelPipelineLayout, "FrameLevelPipelineLayout");

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
    backendDevice->setObjectDebugName(_frameGlobalDescriptorSet, "FrameGlobalDescriptorSet");

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
    // Add pass dependencies
    // After this process we guarantee that every input
    // has a valid producer.
    // And we only allow concurrent read to the same resource.
    
    for(int i = 0; i < _rasterPasses.size();i++)
    {
        auto & pass = _rasterPasses[i];
        std::vector<std::tuple<std::string, std::string, PassResourceDescriptionBase*>> inputResourceTokens;
        inputResourceTokens.reserve(pass->inputs.size() + pass->inouts.size());
        for(int j = 0; j < pass->inputs.size(); j++)
        {
            auto inputName = pass->inputs[j]->name;
            size_t pos = inputName.find("::");
            if(pos == std::string::npos)
                throw std::invalid_argument("Input resource must using PassName as name space");

            auto producerPassName = inputName.substr(0, pos);
            auto resourceName = inputName.substr(pos + 2);
            inputResourceTokens.emplace_back(producerPassName, resourceName, pass->inputs[j].get());
        }

        for (int j = 0; j < pass->inouts.size(); j++)
        {
            auto inoutName = pass->inouts[j]->name;
            size_t pos = inoutName.find("->");
            if (pos == std::string::npos)
                throw std::invalid_argument("Failed to parse inout name");
            auto inputName = inoutName.substr(0, pos);
            if(inputName == "SwapchainImage")
            {
                pass->inouts[j]->outputHandle = swapchainAttachment;
                continue;
            }

            pos = inputName.find("::");
            if (pos == std::string::npos)
                throw std::invalid_argument("Input resource must using PassName as name space");

            auto producerPassName = inputName.substr(0, pos);
            auto resourceName = inputName.substr(pos + 2);
            inputResourceTokens.emplace_back(producerPassName, resourceName, pass->inouts[j].get());
        }

        for (const auto& token : inputResourceTokens)
        {
            auto producerPassName = std::get<0>(token);
            auto resourceName = std::get<1>(token);

            GPUPass* producerPass = nullptr;
            for (int k = 0; k < _rasterPasses.size(); k++)
            {
                if (_rasterPasses[k]->_name == producerPassName)
                {
                    producerPass = _rasterPasses[k].get();
                    break;
                }
            }

            if (producerPass == nullptr)
            {
                throw std::invalid_argument("Can not find pass : " + producerPassName);
            }

            PassResourceDescriptionBase* output = nullptr;
            for (int k = 0; k < producerPass->outputs.size(); k++)
            {
                //todo : check concurrent usage to avoid RW or WW
                if (producerPass->outputs[k]->name == resourceName)
                {
                    output = producerPass->outputs[k].get();
                    std::get<2>(token)->outputHandle = output;
                    break;
                }
            }

            if (output == nullptr)
            {
                for (int k = 0; k < producerPass->inouts.size(); k++)
                {
                    //todo : check concurrent usage to avoid RW or WW
                    auto inoutName = producerPass->inouts[k]->name;
                    size_t pos = inoutName.find("->");
                    if (pos == std::string::npos)
                        throw std::invalid_argument("Failed to parse inout name");
                    auto outputName = inoutName.substr(pos+2);
                    if (outputName == resourceName)
                    {
                        output = producerPass->inouts[k].get();
                        std::get<2>(token)->outputHandle = output;
                        break;
                    }
                }
            }

            if (output == nullptr)
            {
                throw std::invalid_argument(producerPassName.append(" doesn't have output ").append(resourceName));
            }

            producerPass->edges.push_back(pass.get());
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

    for(int idx = 0; idx < sortedIndices.size(); idx++)
    {
        auto & pass = _rasterPasses[sortedIndices[idx]];
        for (auto& input : pass->inputs)
        {
            assert(input->outputHandle);
            connectResources(input->outputHandle.pointer, input.get());
        }
        for (auto& output : pass->outputs)
        {
            if (output->getType() == Attachment)
            {
                // Need to create new image
                auto attachmentDesc = dynamic_cast<PassAttachmentDescription*>(output.get());
                VMAImage image = createBackingImage(attachmentDesc);
                auto name = pass->_name + "::" + output->name;
                backingImages.emplace(name, image);
                // create image view
                auto imageView = createBackingImageView(attachmentDesc,image.image);
                backingImageViews.emplace(name, imageView);
            }
        }
        for (auto& inout : pass->inouts)
        {
            assert(inout->outputHandle);
            connectResources(inout->outputHandle.pointer, inout.get());
            if (inout->getType() == Attachment)
            {
                auto inoutName = inout->name;
                size_t pos = inoutName.find("->");
                if (pos == std::string::npos)
                    throw std::invalid_argument("Failed to parse inout name");
                auto inputName = inoutName.substr(0, pos);
                auto outputName = inoutName.substr(pos + 2);
                auto name = pass->_name + "::" + outputName;
                backingImageViews.emplace(name, backingImageViews[inputName]);
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
        pass->buildFrameBuffer(this);
    }

    for(auto & pass : _rasterPasses)
    {
        managePassInputDescriptorSet(pass.get());
    }

    // Pass ahead of time preparation
    for(auto & pass : _rasterPasses)
    {
        pass->prepareAOT(this);
    }

    prepareDescriptorSetsAOT();

    Window::registerCursorPosCallback([this](double xPos, double yPos){
        this->x = xPos;
        this->y = yPos;
    });

    backendDevice->_swapchain.registerRecreateCallback([this](auto){
        this->update(Event::SWAPCHAIN_RESIZE);
    });
}

void GPUFrame::update(GPUFrame::Event event) {
    if(event == Event::SWAPCHAIN_RESIZE)
    {
        backingImageViews["SwapchainImage"] = backendDevice->_swapchain.get_image_views().value()[frameIdx];
        std::vector<GPURasterizedPass *> needRebuilds;
        for(int idx = 0; idx < sortedIndices.size(); idx ++)
        {
            auto passIdx = sortedIndices[idx];
            auto & pass = _rasterPasses[passIdx];

            for(auto& output : pass->outputs)
            {
                if(output->getType() == Attachment)
                {
                    auto attachmentDesc = dynamic_cast<PassAttachmentDescription*>(output.get());
                    if (auto ptr = std::get_if<PassAttachmentExtent::SwapchainRelative>(&attachmentDesc->extent)) {
                        needRebuilds.push_back(_rasterPasses[passIdx].get());
                        VMAImage newImg = createBackingImage(attachmentDesc);
                        auto name = pass->_name + "::" + output->name;
                        auto oldImg = backingImages[name];
                        backingImages[name] = newImg;
                        backendDevice->deAllocateImage(oldImg.image,oldImg.allocation);
                        auto newImgView = createBackingImageView(attachmentDesc, newImg.image);
                        auto oldImgView = backingImageViews[name];
                        backendDevice->destroy(oldImgView);
                        backingImageViews[name] = newImgView;
                    }
                }
            }

            for(auto& inout : pass->inouts)
            {
                if(inout->outputHandle->getType() == Attachment)
                {
                    if(inout->outputHandle->name == "SwapchainImage")
                    {
                        needRebuilds.push_back(_rasterPasses[passIdx].get());
                        auto inoutName = inout->name;
                        size_t pos = inoutName.find("->");
                        if (pos == std::string::npos)
                            throw std::invalid_argument("Failed to parse inout name");
                        auto outputName = inoutName.substr(pos + 2);
                        auto name = pass->_name + "::" + outputName;
                        backingImageViews[name] = backingImageViews["SwapchainImage"];
                    }
                    auto desc = dynamic_cast<PassAttachmentDescription*>(inout->outputHandle.pointer);
                    if (std::holds_alternative<PassAttachmentExtent::SwapchainRelative>(desc->extent)) {
                        needRebuilds.push_back(_rasterPasses[passIdx].get());
                        auto inoutName = inout->name;
                        size_t pos = inoutName.find("->");
                        if (pos == std::string::npos)
                            throw std::invalid_argument("Failed to parse inout name");
                        auto inputName = inoutName.substr(0, pos);
                        auto outputName = inoutName.substr(pos + 2);
                        auto name = pass->_name + "::" + outputName;
                        backingImageViews[name] = backingImageViews[inputName];
                    }
                }
            }
        }

        std::sort(needRebuilds.begin(), needRebuilds.end());
        // Remove duplicate elements
        auto last = std::unique(needRebuilds.begin(), needRebuilds.end());
        needRebuilds.erase(last, needRebuilds.end());

        for(int i = 0; i < needRebuilds.size(); i++)
        {
            backendDevice->destroy(needRebuilds[i]->frameBuffer);
            needRebuilds[i]->buildFrameBuffer(this);
        }

        std::vector<vk::DescriptorImageInfo> imgInfos;
        std::vector<vk::WriteDescriptorSet> writes;
        for(int idx = 0; idx < sortedIndices.size(); idx ++)
        {
            auto passIdx = sortedIndices[idx];
            auto & pass = _rasterPasses[passIdx];
            for(auto & input : pass->inputs)
            {
                if(input->getType() == Texture && input->outputHandle->getType() == Attachment)
                {
                    auto desc = dynamic_cast<PassAttachmentDescription*>(input->outputHandle.pointer);
                    if (std::holds_alternative<PassAttachmentExtent::SwapchainRelative>(desc->extent)) {
                        vk::WriteDescriptorSet write;
                        write.setDstSet(getManagedDescriptorSet(pass->_name + "InputDescriptorSet"));
                        write.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
                        write.setDescriptorCount(1);
                        vk::DescriptorImageInfo imgInfo{};
                        imgInfo.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
                        imgInfo.setSampler(samplers[0]);
                        imgInfo.setImageView(getBackingImageView(input->name));
                        imgInfos.push_back(imgInfo);
                        writes.push_back(write);
                    }
                }
            }
        }
        for (int i = 0; i < writes.size(); i++)
        {
            writes[i].setPImageInfo(&imgInfos[i]);
        }

        backendDevice->updateDescriptorSets(writes,{});
    }
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

    for(auto & allocator : perThreadMainCommandAllocators)
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
    auto & cmdAllocator = perThreadMainCommandAllocators[threadId];
    auto cmdPrimary = cmdAllocator.getOrAllocateNextPrimary();

    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse);
    cmdPrimary.begin(beginInfo);
    vk::DebugUtilsLabelEXT frameLabel{};
    auto frameLabelName = std::string("Frame ").append(std::to_string(frameIdx));
    frameLabel.setPLabelName(frameLabelName.c_str());
    cmdPrimary.beginDebugUtilsLabelEXT(frameLabel,backendDevice->getDLD());
    cmdPrimary.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,_frameLevelPipelineLayout,0,_frameGlobalDescriptorSet,nullptr);

    cmdPrimary.setViewport(0, backendDevice->_swapchain.getDefaultViewport());
    cmdPrimary.setScissor(0,backendDevice->_swapchain.getDefaultScissor());

    for(int i = 0 ; i < _rasterPasses.size(); i ++)
    {
        _rasterPasses[i]->prepareIncremental(this);
        vk::DebugUtilsLabelEXT passLabel{};
        passLabel.setPLabelName(_rasterPasses[i]->_name.c_str());
        cmdPrimary.beginDebugUtilsLabelEXT(passLabel,backendDevice->getDLD());
        _rasterPasses[i]->record(cmdPrimary,this);
        cmdPrimary.endDebugUtilsLabelEXT(backendDevice->getDLD());
    }
    cmdPrimary.endDebugUtilsLabelEXT(backendDevice->getDLD());
    cmdPrimary.end();
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
        std::shared_ptr<vk::DescriptorImageInfo[]> s(new vk::DescriptorImageInfo[imgInfoCount]);
        baseLayout.imgInfos.swap(s);
    }
    if(bufInfoCount > 0)
    {
        std::shared_ptr<vk::DescriptorBufferInfo[]> s(new vk::DescriptorBufferInfo[bufInfoCount]);
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

    return baseLayout;
}

void GPUFrame::managePassInputDescriptorSet(GPUPass *pass) {
    if(pass->inputs.empty()) return;
    std::vector<vk::DescriptorSetLayoutBinding> inputBindings;
    inputBindings.reserve(pass->inputs.size());
    int imgCount = 0;
    int bufferCount = 0;
    for(int i = 0; i < pass->inputs.size(); i++)
    {
        auto & input = pass->inputs[i];
        assert(input->getType() == PassResourceType::Texture || input->getType() == PassResourceType::Buffer);
        vk::DescriptorSetLayoutBinding binding;
        binding.setStageFlags(vk::ShaderStageFlagBits::eAllGraphics);
        binding.setBinding(i);
        binding.setDescriptorCount(1);
        switch (input->getType()) {
            case PassResourceType::Texture :
                binding.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
                imgCount++;
                break;
            case PassResourceType::Buffer :
                binding.setDescriptorType(vk::DescriptorType::eStorageBuffer);
                bufferCount++;
                break;
            default:
                break;
        }
        inputBindings.emplace_back(binding);
    }

    pass->passInputDescriptorSetLayout = manageDescriptorSet(pass->_name + "InputDescriptorSet",inputBindings);

    getManagedDescriptorSet(pass->_name + "InputDescriptorSet",[this,pass,imgCount,bufferCount](const vk::DescriptorSet& set)
    {
        std::vector<vk::WriteDescriptorSet> writes;
        std::vector<vk::DescriptorImageInfo> imgInfos;
        std::vector<vk::DescriptorBufferInfo> bufferInfos;
        writes.reserve(pass->inputs.size());
        imgInfos.reserve(imgCount);
        bufferInfos.reserve(bufferCount);

        for(int i = 0; i < pass->inputs.size(); i++)
        {
            auto & input = pass->inputs[i];
            assert(input->getType() == PassResourceType::Texture || input->getType() == PassResourceType::Buffer);
            vk::WriteDescriptorSet write;
            write.setDstSet(set);
            write.setDstBinding(i);
            write.setDescriptorCount(1);
            switch (input->getType()) {
                case PassResourceType::Texture :
                {
                    write.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
                    vk::DescriptorImageInfo imgInfo{};
                    imgInfo.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
                    imgInfo.setSampler(samplers[0]);
                    imgInfo.setImageView(getBackingImageView(input->name));
                    imgInfos.push_back(imgInfo);
                    write.setPImageInfo(&imgInfos[imgInfos.size()-1]);
                    break;
                }
                case PassResourceType::Buffer : {
                    write.setDescriptorType(vk::DescriptorType::eStorageBuffer);
                    vk::DescriptorBufferInfo bufInfo{};
                    //todo
                    bufferInfos.push_back(bufInfo);
                    write.setPBufferInfo(&bufferInfos[bufferInfos.size()-1]);
                    break;
                }
                default:
                    break;
            }
            writes.emplace_back(write);
        }
        backendDevice->updateDescriptorSets(writes,{});
    });
}

vk::DescriptorSetLayout GPUFrame::manageDescriptorSet(std::string &&name, const std::vector<vk::DescriptorSetLayoutBinding> &bindings) {
    //Check existing descriptorSet layout
    for(int i = 0 ; i < managedDescriptorSetLayouts.size(); i++)
    {
        auto & layoutRecord = managedDescriptorSetLayouts[i];
        if( layoutRecord.first == bindings)
        {
            layoutRecord.second.emplace_back(name,vk::DescriptorSet{});
            descriptorSetCallbackMap.emplace(name,std::vector<DescriptorSetCallback>{});
            return layoutRecord.first._layout;
        }
    }

    // Need to allocate new descriptorSetLayout Object
    vk::DescriptorSetLayoutCreateInfo layoutCreateInfo{};
    layoutCreateInfo.setBindings(bindings);
    DescriptorSetLayoutExtended newSetLayout(backendDevice->device,layoutCreateInfo);
    managedDescriptorSetLayouts.emplace_back(newSetLayout,std::vector<DescriptorSetRecord>{});
    managedDescriptorSetLayouts.back().second.emplace_back(name,vk::DescriptorSet{});
    descriptorSetCallbackMap.emplace(name,std::vector<DescriptorSetCallback>{});
    return newSetLayout._layout;
}

void GPUFrame::getManagedDescriptorSet(std::string &&setName, const std::function<void(vk::DescriptorSet)> &cb) {
    auto it = descriptorSetCallbackMap.find(setName);
    if(it == descriptorSetCallbackMap.end())
    {
        throw std::runtime_error("Cannot find managed descriptorSet " + setName);
    }
    it->second.emplace_back(cb);
}

void GPUFrame::prepareDescriptorSetsAOT() {
    managedDescriptorPools.reserve(managedDescriptorSetLayouts.size());
    std::vector<vk::DescriptorPoolSize> poolSizes{};

    for(auto & layoutRecord : managedDescriptorSetLayouts) {
        //Create per layout descriptorPool
        vk::DescriptorPoolCreateInfo poolCreateInfo{};
        poolSizes.resize(layoutRecord.first.bindingCount);
        for (int i = 0; i < layoutRecord.first.bindingCount; i++) {
            auto &binding = layoutRecord.first.bindings[i];
            poolSizes[i].setType(binding.descriptorType);
            poolSizes[i].setDescriptorCount(binding.descriptorCount * layoutRecord.second.size());
        }
        poolCreateInfo.setPPoolSizes(poolSizes.data());
        poolCreateInfo.setPoolSizeCount(poolSizes.size());
        poolCreateInfo.setMaxSets(10 * layoutRecord.second.size());
        auto descriptorPool = backendDevice->createDescriptorPool(poolCreateInfo);
        managedDescriptorPools.emplace_back(descriptorPool);
        //allocate DescriptorSets
        vk::DescriptorSetAllocateInfo allocateInfo{};
        allocateInfo.setDescriptorPool(descriptorPool);
        std::vector<vk::DescriptorSetLayout> layouts;
        for(int i = 0; i < layoutRecord.second.size(); i++)
        {
            layouts.push_back(layoutRecord.first._layout);
        }
        allocateInfo.setSetLayouts(layouts);
        allocateInfo.setDescriptorSetCount(layouts.size());
        auto descriptorSets = backendDevice->allocateDescriptorSets(allocateInfo);
        for(int i = 0; i < allocateInfo.descriptorSetCount;i++)
        {
            auto & setRecord = layoutRecord.second[i];
            setRecord.second = descriptorSets[i];
            backendDevice->setObjectDebugName(setRecord.second,setRecord.first.c_str());
            auto it = descriptorSetCallbackMap.find(setRecord.first);
            if(it != descriptorSetCallbackMap.end())
            {
                for(const auto & cb : it->second)
                {
                    cb(setRecord.second);
                }
            }
        }
    }
}

vk::DescriptorSet GPUFrame::getManagedDescriptorSet(std::string && name) const
{
    for(auto & layoutRecord : managedDescriptorSetLayouts)
    {
        for(auto & setRecord : layoutRecord.second)
        {
            if(setRecord.first == name)
                return setRecord.second;
        }
    }

    throw std::runtime_error("Failed to find managed descriptor set " + name);
}
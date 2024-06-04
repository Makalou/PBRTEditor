#include "FrameGraph.hpp"
#include "GPUPass.h"
#include "GPUFrame.hpp"

void FrameGraphTextureDescription::resolveBarriers(GPUFrame* frame)
{
    std::vector<FrameGraphResourceAccessInfo> activeAccessList;
    activeAccessList.reserve(accessList.size());
    for (const auto& access : accessList)
    {
        if (access.pass->is_enabled())
        {
            activeAccessList.push_back(access);
        }
    }

    // For the pass who firstly touches an image, we don't need sync but we may need to transition the layout
    auto * trackedImage = frame->getBackingTrackedImage(name);
    vk::ImageMemoryBarrier2 imb{};
    imb.setImage(trackedImage->image);
    vk::ImageSubresourceRange subresourceRange{};
    subresourceRange.setBaseMipLevel(0);
    subresourceRange.setLevelCount(1);
    subresourceRange.setBaseArrayLayer(0);
    subresourceRange.setLayerCount(1);
    if (VulkanUtil::isDepthStencilFormat(format))
    {
        subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eDepth);
    }
    else {
        subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor);
    }
    imb.setSubresourceRange(subresourceRange);
    imb.setSrcQueueFamilyIndex(vk::QueueFamilyIgnored);
    imb.setDstQueueFamilyIndex(vk::QueueFamilyIgnored);
    imb.setSrcAccessMask(trackedImage->lastAccess);
    imb.setSrcStageMask(trackedImage->lastStage);
    imb.setOldLayout(trackedImage->lastLayout);
    activeAccessList[0].pass->fillImgBarrierInfo(activeAccessList[0].accessType, this, imb.dstStageMask, imb.dstAccessMask, imb.newLayout);
    if (allow_process_in_flight)
    {
        // todo : Assumption : for in_flight_image, we don't need to add execution barrier
        imb.setSrcAccessMask(vk::AccessFlagBits2::eNone);
        imb.setSrcStageMask(vk::PipelineStageFlagBits2::eNone);
    }
    activeAccessList[0].pass->transientImageMemoryBarriers.emplace_back(imb);

    for (int i = 1; i < activeAccessList.size(); i++)
    {
        auto preAccessType = activeAccessList[i - 1].accessType;
        auto postAccessType = activeAccessList[i].accessType;

        bool has_hazard = false;

        has_hazard |= (std::holds_alternative<FrameGraphResourceAccess::Read>(preAccessType) && std::holds_alternative<FrameGraphResourceAccess::Write>(postAccessType));
        has_hazard |= (std::holds_alternative<FrameGraphResourceAccess::Read>(preAccessType) && std::holds_alternative<FrameGraphResourceAccess::RenderTarget>(postAccessType));
        has_hazard |= (std::holds_alternative<FrameGraphResourceAccess::Read>(preAccessType) && std::holds_alternative<FrameGraphResourceAccess::Sample>(postAccessType)); // layout transition need

        has_hazard |= (std::holds_alternative<FrameGraphResourceAccess::Sample>(preAccessType) && std::holds_alternative<FrameGraphResourceAccess::Write>(postAccessType));
        has_hazard |= (std::holds_alternative<FrameGraphResourceAccess::Sample>(preAccessType) && std::holds_alternative<FrameGraphResourceAccess::RenderTarget>(postAccessType));
        has_hazard |= (std::holds_alternative<FrameGraphResourceAccess::Sample>(preAccessType) && std::holds_alternative<FrameGraphResourceAccess::Read>(postAccessType)); // layout transition need

        has_hazard |= (std::holds_alternative<FrameGraphResourceAccess::Write>(preAccessType) && std::holds_alternative<FrameGraphResourceAccess::Write>(postAccessType));
        has_hazard |= (std::holds_alternative<FrameGraphResourceAccess::Write>(preAccessType) && std::holds_alternative<FrameGraphResourceAccess::RenderTarget>(postAccessType));
        has_hazard |= (std::holds_alternative<FrameGraphResourceAccess::Write>(preAccessType) && std::holds_alternative<FrameGraphResourceAccess::Read>(postAccessType));
        has_hazard |= (std::holds_alternative<FrameGraphResourceAccess::Write>(preAccessType) && std::holds_alternative<FrameGraphResourceAccess::Sample>(postAccessType));

        has_hazard |= (std::holds_alternative<FrameGraphResourceAccess::RenderTarget>(preAccessType) && std::holds_alternative<FrameGraphResourceAccess::Write>(postAccessType));
        has_hazard |= (std::holds_alternative<FrameGraphResourceAccess::RenderTarget>(preAccessType) && std::holds_alternative<FrameGraphResourceAccess::RenderTarget>(postAccessType));
        has_hazard |= (std::holds_alternative<FrameGraphResourceAccess::RenderTarget>(preAccessType) && std::holds_alternative<FrameGraphResourceAccess::Read>(postAccessType));
        has_hazard |= (std::holds_alternative<FrameGraphResourceAccess::RenderTarget>(preAccessType) && std::holds_alternative<FrameGraphResourceAccess::Sample>(postAccessType));

        if (has_hazard)
        {
            activeAccessList[i - 1].pass->fillImgBarrierInfo(preAccessType, this, imb.srcStageMask, imb.srcAccessMask, imb.oldLayout);
            activeAccessList[i].pass->fillImgBarrierInfo(postAccessType, this, imb.dstStageMask, imb.dstAccessMask, imb.newLayout);
            activeAccessList[i].pass->transientImageMemoryBarriers.emplace_back(imb);
        }
    }
    // update the last access information for texture
    activeAccessList.back().pass->fillImgBarrierInfo(activeAccessList.back().accessType, this, imb.dstStageMask, imb.dstAccessMask, imb.newLayout);
    trackedImage->lastAccess = imb.dstAccessMask;
    trackedImage->lastStage = imb.dstStageMask;
    trackedImage->lastLayout = imb.newLayout;
}

void FrameGraphBufferDescription::resolveBarriers(GPUFrame* frame)
{
    std::vector<FrameGraphResourceAccessInfo> activeAccessList;
    activeAccessList.reserve(accessList.size());
    for (const auto& access : accessList)
    {
        if (access.pass->is_enabled())
        {
            activeAccessList.push_back(access);
        }
    }
    for (int i = 1; i < activeAccessList.size(); i++)
    {
        auto preAccessType = activeAccessList[i - 1].accessType;
        auto postAccessType = activeAccessList[i].accessType;

        bool has_hazard = false;

        has_hazard |= (std::holds_alternative<FrameGraphResourceAccess::Read>(preAccessType) && std::holds_alternative<FrameGraphResourceAccess::Write>(postAccessType));

        has_hazard |= (std::holds_alternative<FrameGraphResourceAccess::Write>(preAccessType) && std::holds_alternative<FrameGraphResourceAccess::Write>(postAccessType));
        has_hazard |= (std::holds_alternative<FrameGraphResourceAccess::Write>(preAccessType) && std::holds_alternative<FrameGraphResourceAccess::Read>(postAccessType));

        if (has_hazard)
        {

        }
    }
}

GPUPass* FrameGraph::getPass(const std::string& name)
{
    for (auto& pass : this->_allPasses)
    {
        if (pass->_name == name)
        {
            return pass.get();
        }
    }
    return nullptr;
}

void FrameGraph::setBoolVariable(const std::string& name, bool state)
{
    if (*(boolVariables.find(name)->second.val) != state)
    {

    }
    *(boolVariables.find(name)->second.val) = state;
}

void FrameGraph::toggleBoolVariable(const std::string& name)
{
    bool b = *(boolVariables.find(name)->second.val);
    *(boolVariables.find(name)->second.val) = b;
}

void FrameGraph::setSwitchVariable(const std::string& name, std::string&& _case)
{
    if (*(switchVariables.find(name)->second.val) != _case)
    {
    }
    *(switchVariables.find(name)->second.val) = _case;
}

void FrameGraph::disablePass(const std::string& passName)
{
    auto* pass = getPass(passName);
    if (!pass->force_disabled)
    {
        pass->force_disabled = true;
    }
}

void FrameGraph::enablePass(const std::string& passName)
{
    auto* pass = getPass(passName);
    if (pass->force_disabled)
    {
        pass->force_disabled = false;
    }
}

void FrameGraph::executePass(std::unique_ptr<GPUPass>&& pass)
{
    _allPasses.emplace_back(std::move(pass));
}

void FrameGraph::executeWhen(const std::function<bool(void)>& cond, std::unique_ptr<GPUPass>&& pass)
{
    pass->enableCond = cond;
    _allPasses.emplace_back(std::move(pass));
}

FrameGraphTextureDescription* FrameGraph::createOrGetTexture(const std::string& name, vk::Format format, FrameGraphTextureExtentType extentType, bool allow_process_in_flight)
{
    auto tex = std::make_unique<FrameGraphTexture>(name, format, std::move(extentType));
    tex->allow_process_in_flight = allow_process_in_flight;
    auto* tex_ptr = tex.get();
    textureDescriptions.emplace_back(std::move(tex));
    return tex_ptr;
}

FrameGraphTextureDescription* FrameGraph::createOrGetPersistentTexture(const std::string& name, vk::Format format, FrameGraphTextureExtentType extentType)
{
    auto tex = std::make_unique<FrameGraphTexture>(name, format, std::move(extentType));
    tex->is_persistent = true;
    auto* tex_ptr = tex.get();
    textureDescriptions.emplace_back(std::move(tex));
    return tex_ptr;
}

FrameGraphConditionalVariable::BoolVariabel FrameGraph::createOrGetBoolVariable(const std::string& name, bool intial_state)
{
    FrameGraphConditionalVariable::BoolVariabel bv;
    *bv.val = intial_state;
    boolVariables.emplace(name, bv);
    return boolVariables[name];
}

FrameGraphConditionalVariable::SwitchVariable FrameGraph::createOrGetSwitchVariable(const std::string& name, std::string intial_case)
{
    FrameGraphConditionalVariable::SwitchVariable sv;
    *sv.val = intial_case;
    switchVariables.emplace(name, sv);
    return switchVariables[name];
}

void FrameGraph::compileAOT(FrameCoordinator* coordinator)
{
    sortedIndices.reserve(_allPasses.size());
    for (int i = 0; i < _allPasses.size(); i++)
    {
        sortedIndices.push_back(i);
    }

    // allocate resource ahead of time
    for (const auto& texture : textureDescriptions)
    {
        for (auto& accessInfo : texture->accessList)
        {
            coordinator->createBackingImageView(texture.get(), accessInfo, (!texture->is_persistent && texture->allow_process_in_flight));
        }
    }

    for (auto frame : coordinator->inFlightframes)
    {
        frame->createPresentImage();
    }

    for (auto& pass : _allPasses)
    {
        if (pass->getType() == GPUPassType::Graphics)
        {
            auto* graphicsPass = static_cast<GPURasterizedPass*>(pass.get());
            graphicsPass->buildRenderPass(coordinator->backendDevice);
            for (auto frame : coordinator->inFlightframes)
            {
                frame->framebuffers.push_back({ graphicsPass, graphicsPass->buildFrameBuffer(frame) });
            }
        }
    }

    // Manage pass input descriptorSet
    for (auto& pass : _allPasses)
    {
        managePassInputDescriptorSetAOT(coordinator,pass.get());
    }

    // Pass ahead of time preparation
    for (auto& pass : _allPasses)
    {
        pass->prepareAOT(coordinator);
    }

    coordinator->prepareDescriptorSetsAOT();
}

void FrameGraph::buildBarriers(GPUFrame* frame)
{
    for (auto& texture : textureDescriptions)
    {
        texture->resolveBarriers(frame);
    }

    for (auto& buffer : bufferDescriptions)
    {
        buffer->resolveBarriers(frame);
    }

    presentTextureDesc->resolveBarriers(frame);
}

void FrameGraph::managePassInputDescriptorSetAOT(FrameCoordinator* coordinator, GPUPass* pass)
{
    if (pass->reads.empty() && pass->writes.empty() && pass->samples.empty()) return;

    std::vector<vk::DescriptorSetLayoutBinding> inputBindings;
    using WriteResourcePlaceHolder = std::pair<std::string, VulkanWriteDescriptorSet>;
    std::vector< WriteResourcePlaceHolder> writes;
    inputBindings.reserve(pass->reads.size() + pass->writes.size() + pass->samples.size());
    writes.reserve(inputBindings.size());

    bool should_used_in_flight = false;
    vk::DescriptorSetLayoutBinding binding;
    VulkanWriteDescriptorSet write{};
    vk::DescriptorImageInfo imageInfo{};

    switch (pass->getType())
    {
        case GPUPassType::Graphics:
            binding.setStageFlags(vk::ShaderStageFlagBits::eAllGraphics);
            break;
        case GPUPassType::Compute:
            binding.setStageFlags(vk::ShaderStageFlagBits::eCompute);
            break;
        default:
            break;
    }

    for (auto* input : pass->reads)
    {
        should_used_in_flight &= input->allow_process_in_flight;
        binding.setBinding(inputBindings.size());
        binding.setDescriptorCount(1);

        switch (input->getType()) {
            case FrameGraphResourceType::Texture:
                binding.setDescriptorType(vk::DescriptorType::eStorageImage);
                write.descriptorType = vk::DescriptorType::eStorageImage;
                write.dstBinding = binding.binding;
                imageInfo.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
                write.resourceInfo = imageInfo;
                break;
            case FrameGraphResourceType::Buffer:
                binding.setDescriptorType(vk::DescriptorType::eStorageBuffer);
                write.descriptorType = vk::DescriptorType::eStorageBuffer;
                write.dstBinding = binding.binding;
                write.resourceInfo = {};
                //todo
                break;
            default:
                break;
        }
        inputBindings.emplace_back(binding);
        writes.push_back({input->name,write});
    }

    for (auto* input : pass->writes)
    {
        should_used_in_flight &= input->allow_process_in_flight;
        binding.setBinding(inputBindings.size());
        binding.setDescriptorCount(1);
        switch (input->getType()) {
        case FrameGraphResourceType::Texture:
            binding.setDescriptorType(vk::DescriptorType::eStorageImage);
            write.descriptorType = vk::DescriptorType::eStorageImage;
            write.dstBinding = binding.binding;
            imageInfo.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
            write.resourceInfo =imageInfo;
            break;
        case FrameGraphResourceType::Buffer:
            binding.setDescriptorType(vk::DescriptorType::eStorageBuffer);
            write.descriptorType = vk::DescriptorType::eStorageBuffer;
            write.dstBinding = binding.binding;
            write.resourceInfo = {};
            //todo
            break;
        default:
            break;
        }
        inputBindings.emplace_back(binding);
        writes.push_back({ input->name,write });
    }

    for (auto* input : pass->samples)
    {
        should_used_in_flight &= input->allow_process_in_flight;
        binding.setBinding(inputBindings.size());
        binding.setDescriptorCount(1);
        binding.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
        write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        write.dstBinding = binding.binding;
        imageInfo.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
        imageInfo.setSampler(coordinator->samplers[0]);
        write.resourceInfo = imageInfo;
        inputBindings.emplace_back(binding);
        writes.push_back({ input->name,write });
    }

    if (should_used_in_flight)
    {
        pass->passInputDescriptorSetLayout = coordinator->manageInFlightDescriptorSetAOT(pass->_name + "InputDescriptorSet", inputBindings);
        coordinator->updateInFlightDescriptorSetAOT(pass->_name + "InputDescriptorSet", [this, pass,writes](GPUFrame* frame) {
            std::vector<VulkanWriteDescriptorSet> writeDescriptorSets;
            writeDescriptorSets.reserve(writes.size());
            for (auto & placeholder : writes)
            {
                if (std::holds_alternative<vk::DescriptorImageInfo>(placeholder.second.resourceInfo))
                {
                    vk::DescriptorImageInfo imageInfo = std::get<vk::DescriptorImageInfo>(placeholder.second.resourceInfo);
                    imageInfo.setImageView(frame->getBackingImageView(pass->_name + "::" +placeholder.first));
                    auto write = placeholder.second;
                    write.resourceInfo = imageInfo;
                    writeDescriptorSets.push_back(write);
                }
                if (std::holds_alternative<vk::DescriptorBufferInfo>(placeholder.second.resourceInfo))
                {
                    //todo
                }
            }
            return writeDescriptorSets;
        });
    }
    else {

        pass->passInputDescriptorSetLayout = coordinator->manageSharedDescriptorSetAOT(pass->_name + "InputDescriptorSet", inputBindings);

        std::vector<VulkanWriteDescriptorSet> writeDescriptorSets;
        writeDescriptorSets.reserve(writes.size());

        for (auto placeholder : writes)
        {
            if (std::holds_alternative<vk::DescriptorImageInfo>(placeholder.second.resourceInfo))
            {
                vk::DescriptorImageInfo imageInfo = std::get<vk::DescriptorImageInfo>(placeholder.second.resourceInfo);
                imageInfo.setImageView(coordinator->getBackingImageView(pass->_name + "::" + placeholder.first));
                auto write = placeholder.second;
                write.resourceInfo = imageInfo;
                writeDescriptorSets.push_back(write);
            }
            if (std::holds_alternative<vk::DescriptorBufferInfo>(placeholder.second.resourceInfo))
            {
                //todo
            }
        }
        coordinator->updateSharedDescriptorSetAOT(pass->_name + "InputDescriptorSet", writeDescriptorSets);
    }
}
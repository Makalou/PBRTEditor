//
// Created by 王泽远 on 2024/1/18.
//

#include "GPUPass.h"
#include "GPUFrame.hpp"
#include "FrameGraph.hpp"
#include <stack>
#include "window.h"

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

void GPUPass::read(FrameGraphTextureDescription* texture)
{

}

void GPUPass::write(FrameGraphTextureDescription* texture)
{

}

void GPUPass::readwrite(FrameGraphTextureDescription* texture)
{

}

void GPUPass::sample(FrameGraphTextureDescription* texture, bool mipmap)
{
    FrameGraphResourceAccessInfo accessInfo{};
    accessInfo.pass = this;
    accessInfo.accessType = FrameGraphResourceAccess::Sample{ mipmap };
    texture->accessList.emplace_back(std::move(accessInfo));
    this->samples.emplace_back(texture);
}

void GPUPass::prepareIncremental(GPUFrame* frame)
{

}

void GPURasterizedPass::renderTo(FrameGraphTextureDescription* texture, vk::AttachmentLoadOp loadOp)
{
    FrameGraphResourceAccessInfo accessInfo;
    accessInfo.accessType = FrameGraphResourceAccess::RenderTarget{ loadOp,vk::AttachmentStoreOp::eStore };
    accessInfo.pass = this;
    texture->accessList.emplace_back(std::move(accessInfo));
    this->renderTargets.emplace_back(RenderTarget{ loadOp, vk::AttachmentStoreOp::eStore, texture });
}

void GPURasterizedPass::beginPass(vk::CommandBuffer cmdBuf, vk::Framebuffer framebuffer) {
    vk::RenderPassBeginInfo beginInfo{};
    beginInfo.setRenderPass(renderPass);
    beginInfo.setFramebuffer(framebuffer);
    vk::Rect2D renderArea{};
    renderArea.offset.x = 0;
    renderArea.offset.y = 0;
    renderArea.extent.width = framebufferCreateInfo.width;
    renderArea.extent.height = framebufferCreateInfo.height;
    beginInfo.setRenderArea(renderArea);
    //todo hard-code for now
    std::vector<vk::ClearValue> clearValues;
    for (const auto& attachmentDesc : attachmentDescriptions)
    {
        vk::ClearValue clearValue;
        if (VulkanUtil::isDepthStencilFormat(attachmentDesc.format))
        {
            vk::ClearDepthStencilValue value{};
            value.depth = 1.0;
            value.stencil = 0.0;
            clearValue.setDepthStencil(value);
        }
        else {
            vk::ClearColorValue value{};
            value.setFloat32({0.0,0.0,0.0,0.0});
            clearValue.setColor(value);
        }
        clearValues.emplace_back(clearValue);
    }
    beginInfo.setClearValues(clearValues);
    cmdBuf.beginRenderPass(beginInfo,vk::SubpassContents::eInline);
}

void GPURasterizedPass::endPass(vk::CommandBuffer cmdBuf) {
    cmdBuf.endRenderPass();
}

void GPURasterizedPass::buildRenderPass(DeviceExtended* device) {

    attachmentDescriptions.clear();
    for (const auto& renderTarget : renderTargets)
    {
        vk::AttachmentDescription attachment{};
        attachment.setFormat(renderTarget.texture->format);
        attachment.setSamples(vk::SampleCountFlagBits::e1);
        if (VulkanUtil::isDepthStencilFormat(attachment.format))
        {
            attachment.setInitialLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
            attachment.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
        }
        else {
            attachment.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal);
            attachment.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal);
        }
        attachment.setLoadOp(renderTarget.loadOp);
        attachment.setStoreOp(renderTarget.storeOp);
        // We assume currently we don't use stencil attachment
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

    for (int i = 0; i < attachmentDescriptions.size(); i++)
    {
        if (VulkanUtil::isDepthStencilFormat(attachmentDescriptions[i].format))
        {
            depthStencilAttachmentRef.attachment = i;
            //the layout the attachment uses during the subpass.
            depthStencilAttachmentRef.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
        }
        else {
            vk::AttachmentReference colorRef{};
            colorRef.attachment = i;
            colorRef.layout = vk::ImageLayout::eColorAttachmentOptimal;
            colorAttachmentRefs.emplace_back(colorRef);
        }
    }

    subpassInfo.setColorAttachments(colorAttachmentRefs);
    if (depthStencilAttachmentRef.layout != vk::ImageLayout::eUndefined)
    {
        subpassInfo.setPDepthStencilAttachment(&depthStencilAttachmentRef);
    }
    //subpass.setResolveAttachments();
    // We only use one subpass per render pass
    renderPassCreateInfo.setSubpasses(subpassInfo);
    // Since we only have one subpass, we don't care about the dependency ...
    // todo or do we?
    renderPassCreateInfo.setDependencies({});
    renderPass = device->createRenderPass(renderPassCreateInfo);
    device->setObjectDebugName(renderPass, this->_name + "RenderPass");
}

bool GPUComputePass::fillMemBarrierInfo(FrameGraphResourceAccess::Access accessType, FrameGraphBufferDescription* buffer, vk::MemoryBarrier2& barrier)
{
    return false;
}

bool GPUComputePass::fillImgBarrierInfo(FrameGraphResourceAccess::Access accessType, FrameGraphTextureDescription* texture,
    vk::PipelineStageFlags2& stageMask, vk::AccessFlags2& accessMask, vk::ImageLayout& layout)
{
    assert(std::holds_alternative<FrameGraphResourceAccess::Read>(accessType) ||
        std::holds_alternative<FrameGraphResourceAccess::Sample>(accessType) ||
        std::holds_alternative<FrameGraphResourceAccess::Write>(accessType));

    stageMask = vk::PipelineStageFlagBits2::eComputeShader;
    if (std::holds_alternative<FrameGraphResourceAccess::Read>(accessType))
    {
        accessMask = vk::AccessFlagBits2::eShaderRead;
        layout = vk::ImageLayout::eShaderReadOnlyOptimal;
        return true;
    }
    if (std::holds_alternative<FrameGraphResourceAccess::Sample>(accessType))
    {
        accessMask = vk::AccessFlagBits2::eShaderSampledRead;
        layout = vk::ImageLayout::eShaderReadOnlyOptimal;
        return true;
    }
    if (std::holds_alternative<FrameGraphResourceAccess::Write>(accessType))
    {
        accessMask = vk::AccessFlagBits2::eShaderWrite;
        layout = vk::ImageLayout::eGeneral;
        return true;
    }

    return false;
}

/*
 * May need recreate when swapChain invalid
 */
vk::Framebuffer GPURasterizedPass::buildFrameBuffer(GPUFrame* frame) {
    assert(renderPass != VK_NULL_HANDLE);
    framebufferCreateInfo.setRenderPass(renderPass);
    //For now, we assume each attachment has same size
    int width = 0;
    int height = 0;

    std::vector<vk::ImageView> imageViews{};

    for (const auto& renderTarget : renderTargets)
    {
        auto attachmentExtent = renderTarget.texture->getExtent(frame->backendDevice->_swapchain);
        if (width == 0) {
            width = attachmentExtent.width;
        }
        else {
            assert(width == attachmentExtent.width);
        }

        if (height == 0) {
            height = attachmentExtent.height;
        }
        else {
            assert(height == attachmentExtent.height);
        }
        if (renderTarget.texture->name == "PresentImage")
        {
            imageViews.push_back(frame->presentImageView);
        }
        else {
            imageViews.push_back(frame->getBackingImageView(this->_name + "::" + renderTarget.texture->name));
        }
    }

    framebufferCreateInfo.setWidth(width);
    framebufferCreateInfo.setHeight(height);
    framebufferCreateInfo.setLayers(1);
    framebufferCreateInfo.setAttachments(imageViews);
    auto frameBuffer = frame->backendDevice->createFramebuffer(framebufferCreateInfo);
    frame->backendDevice->setObjectDebugName(frameBuffer, "Frame " + std::to_string(frame->frameIdx) + this->_name + "FrameBuffer");
    return frameBuffer;
}

vk::Framebuffer GPURasterizedPass::buildSwapChainTouchedFrameBuffer(DeviceExtended* device, int width, int height, vk::ImageView swapchainImageView)
{
    assert(renderPass != VK_NULL_HANDLE);
    framebufferCreateInfo.setRenderPass(renderPass);

    framebufferCreateInfo.setWidth(width);
    framebufferCreateInfo.setHeight(height);
    framebufferCreateInfo.setLayers(1);
    framebufferCreateInfo.setAttachments(swapchainImageView);
    auto frameBuffer = device->createFramebuffer(framebufferCreateInfo);
    device->setObjectDebugName(frameBuffer, "SwapchainFramebufferFor" + this->_name);
    return frameBuffer;
}

bool GPURasterizedPass::fillMemBarrierInfo(FrameGraphResourceAccess::Access accessType, FrameGraphBufferDescription* buffer, vk::MemoryBarrier2& barrier)
{
    return false;
}

bool GPURasterizedPass::fillImgBarrierInfo(FrameGraphResourceAccess::Access accessType, FrameGraphTextureDescription* texture,
    vk::PipelineStageFlags2& stageMask, vk::AccessFlags2& accessMask, vk::ImageLayout& layout)
{
    if (std::holds_alternative<FrameGraphResourceAccess::Read>(accessType))
    {
        stageMask = vk::PipelineStageFlagBits2::eFragmentShader;
        accessMask = vk::AccessFlagBits2::eShaderRead;
        if (VulkanUtil::isDepthStencilFormat(texture->format))
        {
            layout = vk::ImageLayout::eDepthStencilReadOnlyOptimal;
        }
        else {
            layout = vk::ImageLayout::eShaderReadOnlyOptimal;
        }
        return true;
    }
    if (std::holds_alternative<FrameGraphResourceAccess::Write>(accessType))
    {
        stageMask = vk::PipelineStageFlagBits2::eFragmentShader;
        accessMask = vk::AccessFlagBits2::eShaderRead;
        layout = vk::ImageLayout::eGeneral;
        return true;
    }
    if (std::holds_alternative<FrameGraphResourceAccess::Sample>(accessType))
    {
        stageMask = vk::PipelineStageFlagBits2::eFragmentShader;
        accessMask = vk::AccessFlagBits2::eShaderSampledRead;
        if (VulkanUtil::isDepthStencilFormat(texture->format))
        {
            layout = vk::ImageLayout::eDepthStencilReadOnlyOptimal;
        }
        else {
            layout = vk::ImageLayout::eShaderReadOnlyOptimal;
        }
        return true;
    }
    if (std::holds_alternative<FrameGraphResourceAccess::RenderTarget>(accessType))
    {
        if (VulkanUtil::isDepthStencilFormat(texture->format))
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

void GPUComputePass::record(vk::CommandBuffer cmd, GPUFrame* frame)
{
    for (const auto& ctx : actionContextQueue)
    {
        std::vector<vk::DescriptorSet> vkDescriptorSets;
        vkDescriptorSets.reserve(ctx.descriptorSets.size());
        for (const auto& descriptorSet : ctx.descriptorSets)
        {
            if (std::holds_alternative<std::string>(descriptorSet))
            {
                vkDescriptorSets.push_back(frame->getManagedDescriptorSet(std::get<std::string>(descriptorSet).c_str()));
            }
            else {
                vkDescriptorSets.push_back(std::get<vk::DescriptorSet>(descriptorSet));
            }
        }
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, computePipelines[ctx.pipelineIdx].getLayout(), ctx.firstSet, vkDescriptorSets, nullptr);
        cmd.bindPipeline(vk::PipelineBindPoint::eCompute, computePipelines[ctx.pipelineIdx].getPipeline());
        ctx.action(cmd, ctx.pipelineIdx);
    }
}

void GPURasterizedPass::record(vk::CommandBuffer cmd, GPUFrame* frame)
{
    beginPass(cmd,frame->getFramebufferFor(this));
    for (const auto& ctx : actionContextQueue)
    {
        std::vector<vk::DescriptorSet> vkDescriptorSets;
        vkDescriptorSets.reserve(ctx.descriptorSets.size());
        for (const auto& descriptorSet : ctx.descriptorSets)
        {
            if (std::holds_alternative<std::string>(descriptorSet))
            {
                vkDescriptorSets.push_back(frame->getManagedDescriptorSet(std::get<std::string>(descriptorSet).c_str()));
            }
            else {
                vkDescriptorSets.push_back(std::get<vk::DescriptorSet>(descriptorSet));
            }
        }
        if (ctx.pipelineIdx != -1)
        {
            cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, graphicsPipelines[ctx.pipelineIdx].getLayout(), ctx.firstSet, vkDescriptorSets, nullptr);
            cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipelines[ctx.pipelineIdx].getPipeline());
        }
        ctx.action(cmd,ctx.pipelineIdx);
    }
    endPass(cmd);
}


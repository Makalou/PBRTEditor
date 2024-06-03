#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <variant>
#include "VulkanExtension.h"

#include "FrameGraphResourceDef.hpp"

struct GPUPass;
struct GPUFrame;
struct FrameCoordinator;

struct FrameGraph
{
    FrameGraph(vk::Format swapChainFormat)
    {
        presentTextureDesc = std::make_unique<FrameGraphTexture>("PresentImage", swapChainFormat, FrameGraphTextureExtent::SwapchainRelative(1, 1));
        presentTextureDesc->allow_process_in_flight = true;
        presentTextureDesc->is_persistent = false;
    }

    GPUPass* getPass(const std::string& name);

    void setBoolVariable(const std::string& name, bool state);

    void toggleBoolVariable(const std::string& name);

    void setSwitchVariable(const std::string& name, std::string&& _case);

    void disablePass(const std::string& passName);

    void enablePass(const std::string& passName);

    void executePass(std::unique_ptr<GPUPass>&& pass);

    void executeWhen(const std::function<bool(void)>& cond, std::unique_ptr<GPUPass>&& pass);

    FrameGraphTextureDescription* createOrGetTexture(const std::string& name, vk::Format format, FrameGraphTextureExtentType extentType = FrameGraphTextureExtent::SwapchainRelative{}, bool allow_process_in_flight = false);

    /*When normal texture is used, the user should not have assumption on the inital content of the texture at current frame,
      For persistent texture, the inital content is guaranteed to be the final content from previous frame  */
    FrameGraphTextureDescription* createOrGetPersistentTexture(const std::string& name, vk::Format format, FrameGraphTextureExtentType extentType = FrameGraphTextureExtent::SwapchainRelative{});

    FrameGraphConditionalVariable::BoolVariabel createOrGetBoolVariable(const std::string& name, bool intial_state = true);

    FrameGraphConditionalVariable::SwitchVariable createOrGetSwitchVariable(const std::string& name, std::string intial_case = "");

    FrameGraphTextureDescription* getPresentTexture()
    {
        return presentTextureDesc.get();
    }

    void compileAOT(FrameCoordinator* coordinator);

    void buildBarriers(GPUFrame* frame);

    void managePassInputDescriptorSetAOT(FrameCoordinator* coordinator, GPUPass* pass);

    std::vector<int> sortedIndices;
    std::vector<std::unique_ptr<GPUPass>> _allPasses;

    bool needToRebuildBarriers = false;

    std::unordered_map<std::string, FrameGraphConditionalVariable::BoolVariabel> boolVariables;
    std::unordered_map<std::string, FrameGraphConditionalVariable::SwitchVariable> switchVariables;

    std::unique_ptr<FrameGraphTextureDescription> presentTextureDesc;

    std::vector<std::unique_ptr<FrameGraphTextureDescription>> textureDescriptions;
    std::vector<std::unique_ptr<FrameGraphBufferDescription>> bufferDescriptions;
};
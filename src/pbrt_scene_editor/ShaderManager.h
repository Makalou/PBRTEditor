//
// Created by 王泽远 on 2024/1/17.
//

#ifndef PBRTEDITOR_SHADERMANAGER_H
#define PBRTEDITOR_SHADERMANAGER_H

#include <utility>
#include <vector>
#include <vulkan/vulkan.hpp>

#include <fstream>
#include <chrono>
#include <cstdlib>
#include "Singleton.h"
#include "VulkanExtension.h"
#include "spirv_reflect.h"

struct Shader
{
    const std::string _uuid;
    const vk::ShaderModule _module;
    const vk::ShaderStageFlagBits _stageFlagBits;

    vk::DescriptorSetLayout shaderCustomSetLayout;
    vk::DescriptorSetLayoutCreateInfo shaderCustomSetLayoutInfo;

    vk::PipelineShaderStageCreateInfo getStageCreateInfo() const
    {
        return info;
    }

    Shader(std::string uuid, vk::ShaderModule module, vk::ShaderStageFlagBits stageFlagBits): _uuid(std::move(uuid)),_module(module), _stageFlagBits(stageFlagBits)
    {
        info.setModule(_module);
        info.setStage(_stageFlagBits);
        info.setPName("main");
    }
protected:
    vk::PipelineShaderStageCreateInfo info{};
};

struct VertexShaderReflectedInputAttribute
{
    const vk::VertexInputRate _inputRate;
    const uint32_t _location;
    const vk::Format _format;
    const std::string _name;

    VertexShaderReflectedInputAttribute(vk::VertexInputRate inputRate, uint32_t loc, vk::Format format, std::string  name)
                                        : _inputRate(inputRate), _location(loc),_format(format), _name(std::move(name))
    {

    }
};

struct VertexShader : Shader
{
    VertexShader(std::string uuid,vk::ShaderModule module, const std::vector<VertexShaderReflectedInputAttribute>& reflectedInput)
                : Shader(uuid,module,vk::ShaderStageFlagBits::eVertex), reflectedInputLayout(reflectedInput)
    {

    }

    auto getInputLayout() const
    {
        return reflectedInputLayout;
    }

    const std::vector<VertexShaderReflectedInputAttribute> reflectedInputLayout;
};

struct FragmentShader : Shader
{
    FragmentShader(std::string uuid,vk::ShaderModule module) : Shader(uuid,module,vk::ShaderStageFlagBits::eFragment)
    {

    }
};

struct ComputeShader : Shader
{
    ComputeShader(std::string uuid,vk::ShaderModule module) : Shader(uuid,module,vk::ShaderStageFlagBits::eCompute)
    {

    }
};

struct ShaderManager : Singleton<ShaderManager>
{
public:
    /*
     * Why we still need macro given that we have specialization constants in Vulkan?
     * Because sometimes we want to control the layout, code structure of shader
     * in addition to the execution flow.
     *
     * For example, vertex shader does/doesn't have per vertex normal attribute, uv attribute,
     * normal map on/off, will affect the layout declaration of the shader, which cannot be controlled
     * by specialization constants.
     */
    using ShaderMacro = std::pair<std::string,std::string>;
    using ShaderMacroList = std::vector<ShaderMacro>;

    static std::string queryShaderVariantUUID(const std::string& shaderName, const ShaderMacroList & macro_defs)
    {
        std::string variantSuffix = "@DEFAULT";
        if(!macro_defs.empty())
        {
            variantSuffix = "@";
            for(const auto &def : macro_defs)
            {
                variantSuffix+=(def.first+def.second);
            }
        }
        return shaderName + variantSuffix;
    }

    VertexShader* createVertexShader(DeviceExtended* backendDev, const std::string& fileName, const ShaderMacroList & macro_defs);

    VertexShader* createVertexShader(DeviceExtended* backendDev, const std::string& name);

    FragmentShader* createFragmentShader(DeviceExtended* backendDev,const std::string& name, const std::vector<ShaderMacro> & macro_defs);

    FragmentShader* createFragmentShader(DeviceExtended* backendDev,const std::string& name);

private:
    template<typename T>
    std::vector<T> loadFileBinary(const std::string& fileName)
    {
        std::ifstream file(fileName, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file "+ fileName + ".spv");
        }

        size_t fileSize = static_cast<T>(file.tellg());
        std::vector<T> data(fileSize / sizeof(T));

        file.seekg(0);
        file.read(reinterpret_cast<char*>(data.data()), fileSize);
        file.close();
        return data;
    }
    std::vector<uint32_t> getOrCreateSPIRVVariant(const std::string& fileName, const std::vector<ShaderMacro> & macro_defs);

    std::vector<VertexShader> cachedVertexShaders;
    std::vector<FragmentShader> cachedFragmentShader;
};

#endif //PBRTEDITOR_SHADERMANAGER_H

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
    const vk::ShaderModule _module;
    const vk::ShaderStageFlagBits _stageFlagBits;

    vk::DescriptorSetLayout shaderCustomSetLayout;
    vk::DescriptorSetLayoutCreateInfo shaderCustomSetLayoutInfo;

    vk::PipelineShaderStageCreateInfo getStageCreateInfo() const
    {
        return info;
    }

    Shader(vk::ShaderModule module, vk::ShaderStageFlagBits stageFlagBits): _module(module), _stageFlagBits(stageFlagBits)
    {
        info.setModule(_module);
        info.setStage(_stageFlagBits);
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
    VertexShader(vk::ShaderModule module, const std::vector<VertexShaderReflectedInputAttribute>& reflectedInput)
                : Shader(module,vk::ShaderStageFlagBits::eVertex), reflectedInputLayout(reflectedInput)
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
    FragmentShader(vk::ShaderModule module) : Shader(module,vk::ShaderStageFlagBits::eFragment)
    {

    }
};

struct ComputeShader : Shader
{
    ComputeShader(vk::ShaderModule module) : Shader(module,vk::ShaderStageFlagBits::eCompute)
    {

    }
};

struct ShaderManager : Singleton<ShaderManager>
{
public:
    using ShaderMacro = std::pair<std::string,std::string>;
    using ShaderMacroList = std::vector<ShaderMacro>;
    VertexShader* createVertexShader(DeviceExtended* backendDev, const std::string& fileName, const ShaderMacroList & macro_defs)
    {
        auto spv_data = getOrCreateSPIRVVariant(fileName,macro_defs);
        auto spv_data_size_in_bytes = spv_data.size() * sizeof(decltype(spv_data)::value_type);
        //reflection
        // Generate reflection data for a shader
        SpvReflectShaderModule module;
        auto result = spvReflectCreateShaderModule(spv_data_size_in_bytes, spv_data.data(), &module);
        assert(result == SPV_REFLECT_RESULT_SUCCESS);
        // Enumerate and extract shader's input variables
        uint32_t var_count = 0;
        result = spvReflectEnumerateInputVariables(&module, &var_count, NULL);
        assert(result == SPV_REFLECT_RESULT_SUCCESS);
        auto** input_vars =
                (SpvReflectInterfaceVariable**)malloc(var_count * sizeof(SpvReflectInterfaceVariable*));
        result = spvReflectEnumerateInputVariables(&module, &var_count, input_vars);
        assert(result == SPV_REFLECT_RESULT_SUCCESS);

        std::vector<VertexShaderReflectedInputAttribute> inputs;

        for(int i = 0 ; i < var_count ; i ++)
        {
            vk::VertexInputRate inputRate;
            if(strncmp(input_vars[i]->name, "inVertex", strlen("inVertex")) == 0)
            {
                inputRate = vk::VertexInputRate::eVertex;
            }else if(strncmp(input_vars[i]->name, "inInst", strlen("inInst")) == 0)
            {
                inputRate = vk::VertexInputRate::eInstance;
            }

            std::string name = input_vars[i]->name;
            auto format = static_cast<vk::Format>(input_vars[i]->format);
            VertexShaderReflectedInputAttribute attribute(inputRate,input_vars[i]->location,format,name);
            inputs.push_back(attribute);
        }
        // Destroy the reflection data when no longer required.
        spvReflectDestroyShaderModule(&module);

        vk::ShaderModuleCreateInfo shaderCreateInfo{};
        shaderCreateInfo.setPCode(spv_data.data());
        shaderCreateInfo.setCodeSize(spv_data_size_in_bytes);
        auto shaderCreateRes = backendDev->createShaderModule(shaderCreateInfo);

        VertexShader vertexShader{shaderCreateRes,inputs};
        cachedVertexShaders.push_back(vertexShader);

        return &cachedVertexShaders.back();
    }

    VertexShader* createVertexShader(DeviceExtended* backendDev, const std::string& name)
    {
        return createVertexShader(backendDev,name,{});
    }

    FragmentShader* createFragmentShader(DeviceExtended* backendDev,const std::string& name, const std::vector<ShaderMacro> & macro_defs)
    {

        //preprocess and compile to spir-v

        //reflection
        return nullptr;
    }

    FragmentShader* createFragmentShader(DeviceExtended* backendDev,const std::string& name)
    {
        return createFragmentShader(backendDev,name,{});
    }

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
    std::vector<uint32_t> getOrCreateSPIRVVariant(const std::string& fileName, const std::vector<ShaderMacro> & macro_defs)
    {
        //preprocess and compile to spir-v
        std::filesystem::path searchDir = EDITOR_PROJECT_SOURCE_DIR;
        auto file_abs_dir = searchDir /"res" / "shaders";
        auto file_abs_dir_c_str = file_abs_dir.string();
        auto file_abs_path = file_abs_dir /fileName;
        auto file_abs_path_c_str = file_abs_path.string();

        auto lwt = std::filesystem::last_write_time(file_abs_path);
        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(lwt.time_since_epoch()).count();

        std::string timestampSuffix = "."+std::to_string(timestamp);
        std::string variantSuffix = ".DEFAULT";
        if(!macro_defs.empty())
        {
            variantSuffix = ".";
            for(const auto &def : macro_defs)
            {
                variantSuffix+=(def.first+def.second);
            }
        }
        // If time stamp match and variant match, we directly load the spv file.
        auto expectedFileName = file_abs_path_c_str  + variantSuffix + timestampSuffix + ".spv";
        try {
            return loadFileBinary<uint32_t>(expectedFileName);
        }catch (...){

        }
        // else we delete old compiled file. Compile update to date one.
        // We don't use delete command because it is not cross-platforms


        std::cout.flush();
        std::string command = "glslangValidator -V ";
        if(!macro_defs.empty())
        {
            variantSuffix = ".";
            for(const auto &def : macro_defs)
            {
                command += " -D" + def.first + "=" + def.second + " ";
            }
        }
        command += " -o ";
        command += expectedFileName;
        command += " ";
        command += file_abs_path_c_str;

        if(std::system(command.c_str())!=0)
        {
            //throw std::runtime_error("Failed to run command.");
            std::cerr<< "Failed to run command.\n";
        }
        //load spir-v binary

        return loadFileBinary<uint32_t>(expectedFileName);
    }

    std::vector<VertexShader> cachedVertexShaders;
    std::vector<FragmentShader> cachedFragmentShader;
};

#endif //PBRTEDITOR_SHADERMANAGER_H

//
// Created by 王泽远 on 2024/1/17.
//
#include "ShaderManager.h"

#include <iostream>

VertexShader *ShaderManager::createVertexShader(DeviceExtended *backendDev, const std::string &fileName,
                                                const ShaderManager::ShaderMacroList &macro_defs)
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

VertexShader *ShaderManager::createVertexShader(DeviceExtended *backendDev, const std::string &name)
{
    return createVertexShader(backendDev,name,{});
}

FragmentShader *ShaderManager::createFragmentShader(DeviceExtended *backendDev, const std::string &fileName,
                                                    const std::vector<ShaderMacro> &macro_defs) {
    auto spv_data = getOrCreateSPIRVVariant(fileName,macro_defs);
    auto spv_data_size_in_bytes = spv_data.size() * sizeof(decltype(spv_data)::value_type);
    //todo reflection
    vk::ShaderModuleCreateInfo shaderCreateInfo{};
    shaderCreateInfo.setPCode(spv_data.data());
    shaderCreateInfo.setCodeSize(spv_data_size_in_bytes);
    auto shaderCreateRes = backendDev->createShaderModule(shaderCreateInfo);

    FragmentShader fragmentShader{shaderCreateRes};
    cachedFragmentShader.push_back(fragmentShader);

    return &cachedFragmentShader.back();
}

FragmentShader *ShaderManager::createFragmentShader(DeviceExtended *backendDev, const std::string &name) {
    return createFragmentShader(backendDev,name,{});
}

std::vector<uint32_t>
ShaderManager::getOrCreateSPIRVVariant(const std::string &fileName, const std::vector<ShaderMacro> &macro_defs)
{
    //preprocess and compile to spir-v
    std::filesystem::path searchDir = EDITOR_PROJECT_SOURCE_DIR;
    auto shader_src_abs_dir = searchDir /"res" / "shaders";
    auto shader_compiled_abs_dir = shader_src_abs_dir / "compiled";
    auto shader_src_abs_path = shader_src_abs_dir /fileName;
    auto shader_compiled_abs_path = shader_compiled_abs_dir/fileName;
    auto shader_src_abs_dir_c_str = shader_src_abs_dir.string();
    auto shader_compiled_abs_dir_c_str = shader_compiled_abs_dir.string();
    auto shader_src_abs_path_c_str = shader_src_abs_path.string();
    auto shader_compiled_abs_path_c_str = shader_compiled_abs_path.string();

    auto lwt = std::filesystem::last_write_time(shader_src_abs_path);
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(lwt.time_since_epoch()).count();

    std::string timestampSuffix = "@"+std::to_string(timestamp);
    std::string variantSuffix = "@DEFAULT";
    if(!macro_defs.empty())
    {
        variantSuffix = "@";
        for(const auto &def : macro_defs)
        {
            variantSuffix+=(def.first+def.second);
        }
    }
    // If time stamp match and variant match, we directly load the spv file.
    auto expectedFileName = shader_compiled_abs_path_c_str  + variantSuffix + timestampSuffix + ".spv";
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
    command += shader_src_abs_path_c_str;

    if(std::system(command.c_str())!=0)
    {
        //throw std::runtime_error("Failed to run command.");
        std::cerr<< "Failed to run command.\n";
    }

    //load spir-v binary(Ideally we should cache loaded binary content)
    return loadFileBinary<uint32_t>(expectedFileName);
}


#include "Vultron/Vulkan/VulkanShader.h"
#include "Vultron/Vulkan/VulkanUtils.h"

#include <fstream>
#include <cassert>
#include <vector>
#include <iostream>
#include <filesystem>

namespace Vultron
{
    Ptr<VulkanShader> VulkanShader::CreatePtr(const ShaderCreateInfo &createInfo)
    {
        VkShaderModuleCreateInfo createInfoVk = {};
        createInfoVk.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfoVk.codeSize = createInfo.source.size();
        createInfoVk.pCode = reinterpret_cast<const uint32_t *>(createInfo.source.data());

        VkShaderModule shaderModule;
        VK_CHECK(vkCreateShaderModule(createInfo.device, &createInfoVk, nullptr, &shaderModule));

        return MakePtr<VulkanShader>(shaderModule);
    }

    Ptr<VulkanShader> VulkanShader::CreatePtrFromFile(const ShaderFromFileCreateInfo &createInfo)
    {
        std::ifstream file(createInfo.filepath, std::ios::ate | std::ios::binary);
        assert(file.is_open());

        size_t fileSize = static_cast<size_t>(file.tellg());
        std::vector<char> buffer(fileSize);
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();

        return CreatePtr({.device = createInfo.device, .source = std::string(buffer.data(), fileSize)});
    }

    void VulkanShader::Destroy(VkDevice device)
    {
        vkDestroyShaderModule(device, m_ShaderModule, nullptr);
    }
}
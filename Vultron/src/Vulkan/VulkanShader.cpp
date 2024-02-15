#include "Vultron/Vulkan/VulkanShader.h"

#include <fstream>
#include <cassert>
#include <vector>
#include <iostream>
#include <filesystem>

namespace Vultron
{
    void VulkanShader::Destroy(VkDevice device)
    {
        vkDestroyShaderModule(device, m_ShaderModule, nullptr);
    }

    std::shared_ptr<VulkanShader> VulkanShader::Create(VkDevice device, const std::string &source)
    {
        VkShaderModuleCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = source.size();
        createInfo.pCode = reinterpret_cast<const uint32_t *>(source.data());

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create shader module!");
        }

        std::shared_ptr<VulkanShader> shader = std::make_shared<VulkanShader>();
        shader->m_ShaderModule = shaderModule;

        return shader;
    }

    std::shared_ptr<VulkanShader> VulkanShader::CreateFromFile(VkDevice device, const std::string &filepath)
    {
        std::ifstream file(filepath, std::ios::ate | std::ios::binary);
        // Print absolute path to file
        std::cout << "Filepath: " << std::filesystem::absolute(filepath) << std::endl;
        assert(file.is_open());

        size_t fileSize = static_cast<size_t>(file.tellg());
        std::vector<char> buffer(fileSize);
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();

        return Create(device, std::string(buffer.data(), fileSize));
    }
}
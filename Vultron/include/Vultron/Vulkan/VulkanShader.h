#pragma once

#include "vulkan/vulkan.h"

#include <memory>
#include <string>

namespace Vultron
{
    class VulkanShader
    {
    private:
        VkShaderModule m_ShaderModule;

    public:
        VulkanShader() = default;
        ~VulkanShader() = default;

        VkShaderModule GetShaderModule() const { return m_ShaderModule; }

        void Destroy(VkDevice device);
        static std::shared_ptr<VulkanShader> Create(VkDevice device, const std::string &source);
        static std::shared_ptr<VulkanShader> CreateFromFile(VkDevice device, const std::string &filepath);
    };
}
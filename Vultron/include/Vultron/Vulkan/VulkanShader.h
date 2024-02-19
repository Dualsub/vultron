#pragma once

#include "Vultron/Core/Core.h"

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
        VulkanShader(VkShaderModule shaderModule) : m_ShaderModule(shaderModule) {}
        ~VulkanShader() = default;

        VkShaderModule GetShaderModule() const { return m_ShaderModule; }

        struct ShaderCreateInfo
        {
            VkDevice device = VK_NULL_HANDLE;
            const std::string &source;
        };
        static Ptr<VulkanShader> CreatePtr(const ShaderCreateInfo &createInfo);
        
        struct ShaderFromFileCreateInfo
        {
            VkDevice device = VK_NULL_HANDLE;
            const std::string &filepath;
        };
        static Ptr<VulkanShader> CreatePtrFromFile(const ShaderFromFileCreateInfo &createInfo);
        void Destroy(VkDevice device);
    };
}
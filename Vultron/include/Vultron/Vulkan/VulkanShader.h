#pragma once

#include "Vultron/Core/Core.h"
#include "Vultron/Vulkan/VulkanContext.h"

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
        VulkanShader() = default;
        ~VulkanShader() = default;

        VkShaderModule GetShaderModule() const { return m_ShaderModule; }

        struct ShaderCreateInfo
        {
            const std::string &source;
        };
        static VulkanShader Create(const VulkanContext &context, const ShaderCreateInfo &createInfo);
        static Ptr<VulkanShader> CreatePtr(const VulkanContext &context, const ShaderCreateInfo &createInfo);

        struct ShaderFromFileCreateInfo
        {
            const std::string &filepath;
        };
        static VulkanShader CreateFromFile(const VulkanContext &context, const ShaderFromFileCreateInfo &createInfo);
        static Ptr<VulkanShader> CreatePtrFromFile(const VulkanContext &context, const ShaderFromFileCreateInfo &createInfo);
        void Destroy(const VulkanContext &context);
    };
}
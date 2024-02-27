#pragma once

#include "vulkan/vulkan.h"

namespace Vultron
{
    enum class DescriptorType
    {
        None = 0,
        UniformBuffer,
        CombinedImageSampler,
        StorageBuffer
    };

    struct DescriptorSetLayoutBinding
    {
        uint32_t binding;
        uint32_t descriptorCount = 1;
        DescriptorType type = DescriptorType::None;
        VkShaderStageFlags stageFlags = VK_SHADER_STAGE_ALL;
    };

    struct DescriptorSetBinding
    {
        uint32_t binding;
        DescriptorType type = DescriptorType::None;
        // Image
        VkImageView imageView = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
        // Buffer
        VkBuffer buffer = VK_NULL_HANDLE;
        size_t size = 0;
    };

}
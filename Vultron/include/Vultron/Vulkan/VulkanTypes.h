#pragma once

#include "Vultron/Core/Queue.h"

#include "vulkan/vulkan.h"

#include <optional>

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
        VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        // Buffer
        VkBuffer buffer = VK_NULL_HANDLE;
        size_t size = 0;
    };

    struct ImageTransition
    {
        VkImageMemoryBarrier barrier;
        VkPipelineStageFlags srcStage;
        VkPipelineStageFlags dstStage;
        VkSemaphore semaphore;
    };

    using ImageTransitionQueue = Queue<ImageTransition, 512>;

    struct QueueFamilies
    {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> computeFamily;
        std::optional<uint32_t> transferFamily;
        std::optional<uint32_t> presentFamily;

        QueueFamilies() = default;
        ~QueueFamilies() = default;

        bool IsComplete()
        {
            return graphicsFamily.has_value() && computeFamily.has_value() && transferFamily.has_value() && presentFamily.has_value();
        }
    };

}
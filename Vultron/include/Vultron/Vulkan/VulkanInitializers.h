#pragma once

#include "Vultron/Vulkan/VulkanTypes.h"
#include "Vultron/Vulkan/VulkanBuffer.h"
#include "Vultron/Vulkan/VulkanImage.h"

#include "vulkan/vulkan.h"

#include <vector>

namespace Vultron::VkInit
{
    VkDescriptorSet CreateDescriptorSet(VkDevice device, VkDescriptorPool descriptorPool, VkDescriptorSetLayout layout, const std::vector<DescriptorSetBinding> &bindings);
    VkDescriptorSetLayout CreateDescriptorSetLayout(VkDevice device, const std::vector<DescriptorSetLayoutBinding> &bindingLayouts);
    ImageTransition CreateImageTransitionBarrier(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels = 1, uint32_t layers = 1, uint32_t srcQueueFamily = VK_QUEUE_FAMILY_IGNORED, uint32_t dstQueueFamily = VK_QUEUE_FAMILY_IGNORED);
}
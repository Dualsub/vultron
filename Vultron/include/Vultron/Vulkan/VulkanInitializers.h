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
}
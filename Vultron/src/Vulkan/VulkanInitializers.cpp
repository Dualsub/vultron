#include "Vultron/Vulkan/VulkanInitializers.h"

#include "Vultron/Vulkan/VulkanUtils.h"

#include <iostream>

namespace Vultron::VkInit
{
    VkDescriptorSet CreateDescriptorSet(VkDevice device, VkDescriptorPool descriptorPool, VkDescriptorSetLayout layout, const std::vector<DescriptorSetBinding> &bindings)
    {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = 1;
        VkDescriptorSetLayout descriptorSetLayout = layout;
        allocInfo.pSetLayouts = &descriptorSetLayout;

        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        VkResult res = vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet);
        std::cout << "Descriptor set allocation result: " << res << std::endl;

        std::vector<VkWriteDescriptorSet> descriptorWrites = {};
        descriptorWrites.resize(bindings.size());
        for (size_t i = 0; i < bindings.size(); i++)
        {
            const auto &binding = bindings[i];
            VkWriteDescriptorSet &descriptorWrite = descriptorWrites[i];
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = descriptorSet;
            descriptorWrite.dstBinding = binding.binding;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorCount = 1;

            switch (binding.type)
            {
            case DescriptorType::UniformBuffer:
            {
                VkDescriptorBufferInfo bufferInfo = {};
                bufferInfo.buffer = binding.buffer;
                bufferInfo.offset = 0;
                bufferInfo.range = binding.size;

                descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                descriptorWrite.pBufferInfo = &bufferInfo;
                break;
            }
            case DescriptorType::StorageBuffer:
            {
                VkDescriptorBufferInfo bufferInfo = {};
                bufferInfo.buffer = binding.buffer;
                bufferInfo.offset = 0;
                bufferInfo.range = binding.size;

                descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                descriptorWrite.pBufferInfo = &bufferInfo;
                break;
            }
            case DescriptorType::CombinedImageSampler:
            {
                VkDescriptorImageInfo imageInfo = {};
                imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                imageInfo.imageView = binding.imageView;
                imageInfo.sampler = binding.sampler;

                descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                descriptorWrite.pImageInfo = &imageInfo;
                break;
            }
            default:
                break;
            }
        }

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

        return descriptorSet;
    }

    VkDescriptorSetLayout CreateDescriptorSetLayout(VkDevice device, const std::vector<DescriptorSetLayoutBinding> &bindingLayouts)
    {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        bindings.reserve(bindingLayouts.size());
        for (const auto &layout : bindingLayouts)
        {
            VkDescriptorSetLayoutBinding binding = {};
            binding.binding = layout.binding;
            binding.descriptorCount = layout.descriptorCount;
            binding.stageFlags = layout.stageFlags;
            binding.descriptorType = VkUtil::GetDescriptorType(layout.type);
            bindings.push_back(binding);
        }

        VkDescriptorSetLayoutCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        createInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        createInfo.pBindings = bindings.data();

        VkDescriptorSetLayout descriptorSetLayout;
        VK_CHECK(vkCreateDescriptorSetLayout(device, &createInfo, nullptr, &descriptorSetLayout));

        return descriptorSetLayout;
    }
}
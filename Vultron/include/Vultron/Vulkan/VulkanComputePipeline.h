#pragma once

#include "Vultron/Core/Core.h"
#include "Vultron/Vulkan/VulkanUtils.h"
#include "Vultron/Vulkan/VulkanShader.h"

#include "vulkan/vulkan.h"
#include "vk_mem_alloc.h"

namespace Vultron
{
    class VulkanComputePipeline
    {
    private:
        VkPipeline m_pipeline;
        VkPipelineLayout m_pipelineLayout;
        VkDescriptorSetLayout m_descriptorSetLayout;
        VulkanShader m_shader;

    public:
        VulkanComputePipeline() = default;
        VulkanComputePipeline(VkPipeline pipeline, VkPipelineLayout pipelineLayout, VkDescriptorSetLayout descriptorSetLayout, const VulkanShader &shader)
            : m_pipeline(pipeline), m_pipelineLayout(pipelineLayout), m_descriptorSetLayout(descriptorSetLayout), m_shader(shader)
        {
        }
        ~VulkanComputePipeline() = default;

        struct ComputePipelineCreateInfo
        {
            const VulkanShader &shader;
            const std::vector<DescriptorSetLayoutBinding> &bindings;
            const std::vector<VkPushConstantRange> &pushConstantRanges = {};
        };
        static VulkanComputePipeline Create(const VulkanContext &context, const ComputePipelineCreateInfo &createInfo);
        void Destroy(const VulkanContext &context);

        const VkPipeline &GetPipeline() const { return m_pipeline; }
        const VkPipelineLayout &GetPipelineLayout() const { return m_pipelineLayout; }
        const VkDescriptorSetLayout &GetDescriptorSetLayout() const { return m_descriptorSetLayout; }
        const VulkanShader &GetShader() const { return m_shader; }
    };

}
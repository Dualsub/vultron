#include "Vultron/Vulkan/VulkanComputePipeline.h"

#include "Vultron/Vulkan/VulkanInitializers.h"

namespace Vultron
{
    VulkanComputePipeline VulkanComputePipeline::Create(const VulkanContext &context, const ComputePipelineCreateInfo &createInfo)
    {
        VkDescriptorSetLayout descriptorSetLayout = VkInit::CreateDescriptorSetLayout(context.GetDevice(), createInfo.bindings);

        // Create pipeline layout
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(createInfo.pushConstantRanges.size());
        pipelineLayoutInfo.pPushConstantRanges = createInfo.pushConstantRanges.size() > 0 ? createInfo.pushConstantRanges.data() : nullptr;

        VkPipelineLayout pipelineLayout;
        VK_CHECK(vkCreatePipelineLayout(context.GetDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout));

        // Create compute pipeline
        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        pipelineInfo.stage.module = createInfo.shader.GetShaderModule();
        pipelineInfo.stage.pName = "main";
        pipelineInfo.layout = pipelineLayout;

        VkPipeline pipeline;
        VK_CHECK(vkCreateComputePipelines(context.GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline));

        return VulkanComputePipeline(pipeline, pipelineLayout, descriptorSetLayout, createInfo.shader);
    }

    void VulkanComputePipeline::Destroy(const VulkanContext &context)
    {
        vkDestroyDescriptorSetLayout(context.GetDevice(), m_descriptorSetLayout, nullptr);
        vkDestroyPipelineLayout(context.GetDevice(), m_pipelineLayout, nullptr);
        vkDestroyPipeline(context.GetDevice(), m_pipeline, nullptr);
    }
}
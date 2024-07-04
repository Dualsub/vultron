#include "Vultron/Vulkan/VulkanMaterial.h"

#include "Vultron/Vulkan/VulkanUtils.h"
#include "Vultron/Vulkan/VulkanInitializers.h"

#include <cassert>
#include <iostream>

namespace Vultron
{
    VulkanMaterialPipeline VulkanMaterialPipeline::Create(const VulkanContext &context, const VulkanRenderPass &renderPass, const MaterialCreateInfo &createInfo)
    {
        bool shouldBindMaterial = createInfo.bindings.size() > 0;
        VulkanMaterialPipeline material(createInfo.vertexShader, createInfo.fragmentShader, shouldBindMaterial);

        if (!material.InitializeDescriptorSetLayout(context, createInfo.bindings))
        {
            std::cerr << "Failed to initialize descriptor set layout" << std::endl;
            assert(false);
        }

        if (!material.InitializeGraphicsPipeline(
                context, renderPass,
                createInfo.vertexDescription,
                createInfo.descriptorSetLayouts,
                createInfo.pushConstantRanges,
                createInfo.cullMode,
                createInfo.depthFunction,
                createInfo.depthTestEnable,
                createInfo.depthWriteEnable))
        {
            std::cerr << "Failed to initialize graphics pipeline" << std::endl;
            assert(false);
        }

        return material;
    }

    void VulkanMaterialPipeline::Destroy(const VulkanContext &context)
    {
        if (m_descriptorSetLayout != VK_NULL_HANDLE)
            vkDestroyDescriptorSetLayout(context.GetDevice(), m_descriptorSetLayout, nullptr);
        vkDestroyPipelineLayout(context.GetDevice(), m_pipelineLayout, nullptr);
        vkDestroyPipeline(context.GetDevice(), m_pipeline, nullptr);
    }

    bool VulkanMaterialPipeline::InitializeDescriptorSetLayout(const VulkanContext &context, const std::vector<DescriptorSetLayoutBinding> &bindings)
    {
        if (bindings.size() == 0)
        {
            m_descriptorSetLayout = VK_NULL_HANDLE;
            return true;
        }

        m_descriptorSetLayout = VkInit::CreateDescriptorSetLayout(context.GetDevice(), bindings);
        return true;
    }

    bool VulkanMaterialPipeline::InitializeGraphicsPipeline(const VulkanContext &context, const VulkanRenderPass &renderPass, const VertexDescription &vertexDescription, const std::vector<VkDescriptorSetLayout> &descriptorSetLayouts, const std::vector<VkPushConstantRange> &pushConstantRanges, CullMode cullMode, DepthFunction depthFunction, bool depthTestEnable, bool depthWriteEnable)
    {
        VkPipelineShaderStageCreateInfo shaderStages[] = {
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = m_vertexShader.GetShaderModule(),
                .pName = "main",
            },
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = m_fragmentShader.GetShaderModule(),
                .pName = "main",
            }};

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        if (vertexDescription.attributeDescriptions.size() > 0)
        {

            vertexInputInfo.vertexBindingDescriptionCount = 1;
            vertexInputInfo.pVertexBindingDescriptions = &vertexDescription.bindingDescription;
            vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexDescription.attributeDescriptions.size());
            vertexInputInfo.pVertexAttributeDescriptions = vertexDescription.attributeDescriptions.data();
        }
        else
        {
            vertexInputInfo.vertexBindingDescriptionCount = 0;
            vertexInputInfo.pVertexBindingDescriptions = nullptr;
            vertexInputInfo.vertexAttributeDescriptionCount = 0;
            vertexInputInfo.pVertexAttributeDescriptions = nullptr;
        }

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        const std::array<VkDynamicState, 2> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR};

        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = static_cast<VkCullModeFlags>(cullMode);
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

        std::vector<VkDescriptorSetLayout> layouts = descriptorSetLayouts;
        if (m_descriptorSetLayout != VK_NULL_HANDLE)
            layouts.push_back(m_descriptorSetLayout);

        pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(layouts.size());
        pipelineLayoutInfo.pSetLayouts = layouts.data();
        pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());
        pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges.size() > 0 ? pushConstantRanges.data() : nullptr;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthCompareOp = static_cast<VkCompareOp>(depthFunction);

        if (depthTestEnable)
        {
            depthStencil.depthTestEnable = VK_TRUE;
            depthStencil.depthWriteEnable = VK_TRUE;
            depthStencil.depthBoundsTestEnable = VK_FALSE;
            depthStencil.stencilTestEnable = VK_FALSE;
        }
        else
        {
            depthStencil.depthTestEnable = VK_FALSE;
            depthStencil.depthWriteEnable = VK_FALSE;
            depthStencil.depthBoundsTestEnable = VK_FALSE;
            depthStencil.stencilTestEnable = VK_FALSE;
        }

        if (depthWriteEnable)
        {
            depthStencil.depthWriteEnable = VK_TRUE;
        }
        else
        {
            depthStencil.depthWriteEnable = VK_FALSE;
        }

        VK_CHECK(vkCreatePipelineLayout(context.GetDevice(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout));

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = m_pipelineLayout;
        pipelineInfo.renderPass = renderPass.GetRenderPass();
        pipelineInfo.subpass = 0;

        VK_CHECK(vkCreateGraphicsPipelines(context.GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline));

        return true;
    }

    VulkanMaterialInstance VulkanMaterialInstance::Create(const VulkanContext &context, VkDescriptorPool descriptorPool, const VulkanMaterialPipeline &pipeline, const MaterialInstanceCreateInfo &createInfo)
    {
        auto descriptorSet = VkInit::CreateDescriptorSet(context.GetDevice(), descriptorPool, pipeline.GetDescriptorSetLayout(), createInfo.bindings);
        return VulkanMaterialInstance(descriptorSet);
    }

}
#pragma once

#include "Vultron/Types.h"
#include "Vultron/Vulkan/VulkanTypes.h"
#include "Vultron/Vulkan/VulkanRenderPass.h"
#include "Vultron/Vulkan/VulkanSwapchain.h"
#include "Vultron/Vulkan/VulkanShader.h"

#include "vulkan/vulkan.h"

#include <string>

namespace Vultron
{
    enum class CullMode
    {
        None = VK_CULL_MODE_NONE,
        Front = VK_CULL_MODE_FRONT_BIT,
        Back = VK_CULL_MODE_BACK_BIT,
    };

    enum class DepthFunction
    {
        Never = VK_COMPARE_OP_NEVER,
        Less = VK_COMPARE_OP_LESS,
        Equal = VK_COMPARE_OP_EQUAL,
        LessOrEqual = VK_COMPARE_OP_LESS_OR_EQUAL,
        Greater = VK_COMPARE_OP_GREATER,
        NotEqual = VK_COMPARE_OP_NOT_EQUAL,
        GreaterOrEqual = VK_COMPARE_OP_GREATER_OR_EQUAL,
        Always = VK_COMPARE_OP_ALWAYS,
    };

    class VulkanMaterialPipeline
    {
    private:
        VulkanShader m_vertexShader{};
        VulkanShader m_fragmentShader{};
        VkPipeline m_pipeline{VK_NULL_HANDLE};
        VkPipelineLayout m_pipelineLayout{VK_NULL_HANDLE};
        VkDescriptorSetLayout m_descriptorSetLayout{VK_NULL_HANDLE};
        bool m_shouldBindMaterial = true;

        bool InitializeGraphicsPipeline(const VulkanContext &context, const VulkanRenderPass &renderPass, const VertexDescription &vertexDescription, VkDescriptorSetLayout sceneDescriptorSetLayout, const std::vector<VkPushConstantRange> &pushConstantRanges, CullMode cullMode, DepthFunction depthFunction, bool depthTestEnable, bool depthWriteEnable);
        bool InitializeDescriptorSetLayout(const VulkanContext &context, const std::vector<DescriptorSetLayoutBinding> &descriptorSetLayoutBindings);

    public:
        VulkanMaterialPipeline(const VulkanShader &vertexShader, const VulkanShader &fragmentShader, bool shouldBindMaterial)
            : m_vertexShader(vertexShader), m_fragmentShader(fragmentShader), m_shouldBindMaterial(shouldBindMaterial)
        {
        }
        VulkanMaterialPipeline() = default;
        ~VulkanMaterialPipeline() = default;

        struct MaterialCreateInfo
        {
            const VulkanShader &vertexShader;
            const VulkanShader &fragmentShader;
            VkDescriptorSetLayout sceneDescriptorSetLayout;
            const std::vector<DescriptorSetLayoutBinding> &bindings;
            const std::vector<VkPushConstantRange> &pushConstantRanges = {};
            const VertexDescription &vertexDescription = StaticMeshVertex::GetVertexDescription();
            CullMode cullMode = CullMode::Back;
            DepthFunction depthFunction = DepthFunction::Less;
            bool depthTestEnable = true;
            bool depthWriteEnable = true;
        };

        static VulkanMaterialPipeline Create(const VulkanContext &context, const VulkanRenderPass &renderPass, const MaterialCreateInfo &createInfo);
        void Destroy(const VulkanContext &context);

        VulkanShader GetVertexShader() const { return m_vertexShader; }
        VulkanShader GetFragmentShader() const { return m_fragmentShader; }
        VkPipeline GetPipeline() const { return m_pipeline; }
        VkPipelineLayout GetPipelineLayout() const { return m_pipelineLayout; }
        VkDescriptorSetLayout GetDescriptorSetLayout() const { return m_descriptorSetLayout; }

        bool ShouldBindMaterial() const { return m_shouldBindMaterial; }
    };

    class VulkanMaterialInstance
    {
    private:
        VkDescriptorSet m_descriptorSet;

    public:
        VulkanMaterialInstance(VkDescriptorSet descriptorSet)
            : m_descriptorSet(descriptorSet)
        {
        }
        VulkanMaterialInstance() = default;
        ~VulkanMaterialInstance() = default;

        struct MaterialInstanceCreateInfo
        {
            const std::vector<DescriptorSetBinding> &bindings;
        };

        static VulkanMaterialInstance Create(const VulkanContext &context, VkDescriptorPool descriptorPool, const VulkanMaterialPipeline &pipeline, const MaterialInstanceCreateInfo &createInfo);

        VkDescriptorSet GetDescriptorSet() const { return m_descriptorSet; }
    };
}
#pragma once

#include "Vultron/Vulkan/VulkanContext.h"

#include "vulkan/vulkan.h"

namespace Vultron
{
    class VulkanRenderPass
    {
    private:
        VkRenderPass m_renderPass;

    public:
        VulkanRenderPass(VkRenderPass renderPass) : m_renderPass(renderPass) {}
        VulkanRenderPass() = default;
        ~VulkanRenderPass() = default;

        enum class AttachmentType
        {
            None = 0,
            Color,
            Depth
        };

        struct AttachmentCreateInfo
        {
            AttachmentType type = AttachmentType::Color;
            VkFormat format = VK_FORMAT_UNDEFINED;
            VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
            VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            VkAttachmentLoadOp stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            VkAttachmentStoreOp stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            VkImageLayout finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        };

        struct RenderPassCreateInfo
        {
            const std::vector<AttachmentCreateInfo> &attachments;
        };

        static VulkanRenderPass Create(const VulkanContext &context, const RenderPassCreateInfo &createInfo);
        void Destroy(const VulkanContext &context);

        VkRenderPass GetRenderPass() const { return m_renderPass; }
    };
}

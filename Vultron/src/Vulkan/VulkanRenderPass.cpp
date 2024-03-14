#include "Vultron/Vulkan/VulkanRenderPass.h"

#include "Vultron/Vulkan/VulkanUtils.h"

#include <optional>

namespace Vultron
{
    VulkanRenderPass VulkanRenderPass::Create(const VulkanContext &context, const RenderPassCreateInfo &createInfo)
    {
        std::vector<VkAttachmentDescription> attachments;
        std::vector<VkAttachmentReference> colorAttachmentRefs;
        std::optional<VkAttachmentReference> depthAttachmentRef;

        for (const auto &attachment : createInfo.attachments)
        {
            VkAttachmentDescription desc = {};
            desc.format = attachment.format;
            desc.samples = attachment.samples;
            desc.loadOp = attachment.loadOp;
            desc.storeOp = attachment.storeOp;
            desc.stencilLoadOp = attachment.stencilLoadOp;
            desc.stencilStoreOp = attachment.stencilStoreOp;
            desc.initialLayout = attachment.initialLayout;
            desc.finalLayout = attachment.finalLayout;

            attachments.push_back(desc);

            VkAttachmentReference ref = {};
            ref.attachment = static_cast<uint32_t>(attachments.size() - 1);

            switch (attachment.type)
            {
            case AttachmentType::Color:
                ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                colorAttachmentRefs.push_back(ref);
                break;

            case AttachmentType::Depth:
                ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                depthAttachmentRef = ref;
                break;

            default:
                break;
            }
        }

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = static_cast<uint32_t>(colorAttachmentRefs.size());
        subpass.pColorAttachments = colorAttachmentRefs.data();
        if (depthAttachmentRef.has_value())
        {
            subpass.pDepthStencilAttachment = &depthAttachmentRef.value();
        }

        std::vector<VkSubpassDependency> dependencies;

        if (createInfo.dependencies.size() > 0)
        {
            dependencies = createInfo.dependencies;
        }
        else
        {
            dependencies.push_back({
                .srcSubpass = VK_SUBPASS_EXTERNAL,
                .dstSubpass = 0,
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                .srcAccessMask = 0,
                .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            });
        }

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
        renderPassInfo.pDependencies = dependencies.data();

        VkRenderPass renderPass{};
        VK_CHECK(vkCreateRenderPass(context.GetDevice(), &renderPassInfo, nullptr, &renderPass));

        return VulkanRenderPass(renderPass);
    }

    void VulkanRenderPass::Destroy(const VulkanContext &context)
    {
        vkDestroyRenderPass(context.GetDevice(), m_renderPass, nullptr);
    }
}

#pragma once

#include "Vultron/Window.h"
#include "Vultron/Vulkan/VulkanContext.h"

#include "vulkan/vulkan.h"

#include <vector>

namespace Vultron
{
    class VulkanSwapchain
    {
    private:
        VkSwapchainKHR m_swapchain;
        std::vector<VkImage> m_swapchainImages;
        std::vector<VkImageView> m_swapchainImageViews;
        std::vector<VkFramebuffer> m_swapchainFramebuffers;
        VkFormat m_swapchainImageFormat;
        VkExtent2D m_swapchainExtent;

        bool InitializeSwapChain(const VulkanContext &context, uint32_t width, uint32_t height);
        bool InitializeImageViews(const VulkanContext &context);

    public:
        VulkanSwapchain() = default;
        ~VulkanSwapchain() = default;

        bool Initialize(const VulkanContext &context, uint32_t width, uint32_t height);
        void Destroy(const VulkanContext &context);

        inline VkSwapchainKHR GetSwapchain() const { return m_swapchain; }
        inline const std::vector<VkImage> &GetImages() const { return m_swapchainImages; }
        inline const std::vector<VkImageView> &GetImageViews() const { return m_swapchainImageViews; }
        inline const std::vector<VkFramebuffer> &GetFramebuffers() const { return m_swapchainFramebuffers; }
        inline std::vector<VkFramebuffer> &GetFramebuffers() { return m_swapchainFramebuffers; }
        inline VkFramebuffer GetFramebuffer(uint32_t index) const { return m_swapchainFramebuffers[index]; }
        inline VkFormat GetImageFormat() const { return m_swapchainImageFormat; }
        inline VkExtent2D GetExtent() const { return m_swapchainExtent; }
    };
}
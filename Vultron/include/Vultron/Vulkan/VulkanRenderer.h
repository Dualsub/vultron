#pragma once

#include "Vultron/Core/Core.h"
#include "Vultron/Types.h"
#include "Vultron/Window.h"
#include "Vultron/Vulkan/VulkanUtils.h"
#include "Vultron/Vulkan/VulkanBuffer.h"
#include "Vultron/Vulkan/VulkanImage.h"
#include "Vultron/Vulkan/VulkanMesh.h"
#include "Vultron/Vulkan/VulkanShader.h"

#include "vk_mem_alloc.h"
#include "vulkan/vulkan.h"

#include <array>
#include <optional>

namespace Vultron
{
    struct FrameData
    {
        VkSemaphore imageAvailableSemaphore;
        VkSemaphore renderFinishedSemaphore;
        VkFence inFlightFence;

        VkCommandBuffer commandBuffer;

        VulkanBuffer instanceBuffer;
        uint32_t instanceCount = 0;

        VulkanBuffer uniformBuffer;
        VkDescriptorSet descriptorSet;
    };

    struct InstanceData
    {
        glm::mat4 model;
    };

    struct UniformBufferData
    {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 proj;
        glm::vec3 lightDir;
        float _padding1;
        glm::vec3 viewPos;
        float _padding2;
    };

    static_assert(sizeof(UniformBufferData) % 16 == 0);

    constexpr size_t c_maxInstances = 1000;
    constexpr uint32_t c_frameOverlap = 2;

    const std::vector<const char *> c_deviceExtensions =
        {
#if __APPLE__
            "VK_KHR_portability_subset",
#endif
            VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
            VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    constexpr std::array<const char *, 1> c_validationLayers = {"VK_LAYER_KHRONOS_validation"};
    constexpr bool c_validationLayersEnabled = false;

    class ResourcePool
    {
        std::unordered_map<RenderHandle, VulkanMesh> m_meshes;
        std::unordered_map<RenderHandle, VulkanImage> m_images;

        RenderHandle m_meshCounter = 0;
        RenderHandle m_imageCounter = 0;

    public:
        ResourcePool() = default;
        ~ResourcePool() = default;

        RenderHandle AddMesh(const VulkanMesh &mesh)
        {
            m_meshes.insert({m_meshCounter, mesh});
            return m_meshCounter++;
        }

        RenderHandle AddImage(const VulkanImage &image)
        {
            m_images.insert({m_imageCounter, image});
            return m_imageCounter++;
        }

        const VulkanMesh &GetMesh(RenderHandle id) const { return m_meshes.at(id); }
        const VulkanImage &GetImage(RenderHandle id) const { return m_images.at(id); }

        void Destroy(VkDevice device, VmaAllocator allocator)
        {
            for (auto &mesh : m_meshes)
            {
                mesh.second.Destroy(allocator);
            }

            m_meshes.clear();

            for (auto &image : m_images)
            {
                image.second.Destroy(device, allocator);
            }

            m_images.clear();
        }
    };

    class VulkanRenderer
    {
    private:
        // Vulkan
        VkInstance m_instance;
        VkPhysicalDevice m_physicalDevice;
        VkPhysicalDeviceProperties m_deviceProperties;
        VkDevice m_device;

        // Queues
        VkQueue m_graphicsQueue;
        uint32_t m_graphicsQueueFamily;
        VkQueue m_presentQueue;

        // Swap chain
        VkSurfaceKHR m_surface;
        VkSwapchainKHR m_swapChain;
        std::vector<VkImage> m_swapChainImages;
        std::vector<VkImageView> m_swapChainImageViews;
        std::vector<VkFramebuffer> m_swapChainFramebuffers;
        VkFormat m_swapChainImageFormat;
        VkExtent2D m_swapChainExtent;

        // Render pass
        VkRenderPass m_renderPass;

        // Pipeline
        VkPipelineLayout m_pipelineLayout;
        VkPipeline m_graphicsPipeline;

        // Samplers
        VkSampler m_textureSampler;
        VkSampler m_depthSampler;

        // Depth buffer
        VulkanImage m_depthImage;

        // Descriptor set
        VkDescriptorSetLayout m_descriptorSetLayout;
        VkDescriptorPool m_descriptorPool;

        // Uniform buffer
        UniformBufferData m_uniformBufferData{};

        // Command pool
        VkCommandPool m_commandPool;

        // Allocator
        VmaAllocator m_allocator;

        // Debugging
        VkDebugUtilsMessengerEXT m_debugMessenger;

        // Frame data
        FrameData m_frames[c_frameOverlap];
        uint32_t m_currentFrameIndex = 0;

        // Assets, will be removed in the future
        VulkanShader m_vertexShader;
        VulkanShader m_fragmentShader;
        VulkanImage m_texture;

        ResourcePool m_resourcePool;

        bool InitializeInstance(const Window &window);
        bool InitializeSurface(const Window &window);
        bool InitializePhysicalDevice();
        bool InitializeLogicalDevice();
        bool InitializeSwapChain(uint32_t width, uint32_t height);
        bool InitializeImageViews();
        bool InitializeRenderPass();
        bool InitializeDescriptorSetLayout();
        bool InitializeGraphicsPipeline();
        bool InitializeFramebuffers();
        bool InitializeCommandPool();
        bool InitializeCommandBuffer();
        bool InitializeSyncObjects();
        bool InitializeAllocator();
        // TODO: Remove this in the future
        bool InitializeTestResources();
        bool InitializeSamplers();
        bool InitializeDepthBuffer();
        bool InitializeUniformBuffers();
        bool InitializeInstanceBuffer();
        bool InitializeDescriptorPool();
        bool InitializeDescriptorSets();

        void RecreateSwapChain(uint32_t width, uint32_t height);
        bool CheckDeviceExtensionSupport(VkPhysicalDevice device);

        // Validation/debugging
        bool InitializeDebugMessenger();
        bool CheckValidationLayerSupport();

        void WriteCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, const std::vector<RenderBatch> &batches);

    public:
        VulkanRenderer() = default;
        ~VulkanRenderer() = default;

        bool Initialize(const Window &window);
        void Draw(const std::vector<RenderBatch> &batches, const std::vector<glm::mat4> &instances);
        void Shutdown();

        RenderHandle LoadMesh(const std::string &filepath);
    };

}

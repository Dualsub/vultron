#pragma once

#include "Vultron/Vulkan/VulkanContext.h"
#include "Vultron/Vulkan/VulkanImage.h"
#include "Vultron/Vulkan/VulkanMesh.h"

namespace Vultron
{
    class VulkanEnvironmentMap
    {
    private:
        VulkanImage m_image;
        VulkanImage m_irradiance;
        VulkanImage m_prefiltered;

        VkDescriptorSet m_environmentSet;
        VkDescriptorSet m_skyboxSet;

        static VulkanImage GenerateIrradianceMap(const VulkanContext &context, VkCommandPool commandPool, VkDescriptorPool descriptorPool, const VulkanMesh &skyboxMesh, const VulkanImage &environmentMap);
        static VulkanImage GeneratePrefilteredMap(const VulkanContext &context, VkCommandPool commandPool, VkDescriptorPool descriptorPool, const VulkanMesh &skyboxMesh, const VulkanImage &environmentMap);

        bool InitializeDescriptorSets(const VulkanContext &context, VkDescriptorPool descriptorPool, VkDescriptorSetLayout environmentLayout, VkDescriptorSetLayout skyboxLayout, VkSampler sampler);

    public:
        VulkanEnvironmentMap(const VulkanImage &image, const VulkanImage &irradiance, const VulkanImage &prefiltered)
            : m_image(image), m_irradiance(irradiance), m_prefiltered(prefiltered) {}

        VulkanEnvironmentMap() = default;
        ~VulkanEnvironmentMap() = default;

        struct EnvironmentMapCreateInfo
        {
            const std::string &filepath;
        };

        static VulkanEnvironmentMap CreateFromFile(const VulkanContext &context, VkCommandPool commandPool, VkDescriptorPool descriptorPool, const VulkanMesh &skyboxMesh, VkDescriptorSetLayout environmentLayout, VkDescriptorSetLayout skyboxLayout, VkSampler sampler, const EnvironmentMapCreateInfo &info);
        void Destroy(const VulkanContext &context);

        const VulkanImage &GetImage() const { return m_image; }
        const VulkanImage &GetIrradiance() const { return m_irradiance; }
        const VulkanImage &GetPrefiltered() const { return m_prefiltered; }

        const VkDescriptorSet &GetEnvironmentDescriptorSet() const { return m_environmentSet; }
        const VkDescriptorSet &GetSkyboxDescriptorSet() const { return m_skyboxSet; }
    };
}
#pragma once

#include "Vultron/Types.h"
#include "Vultron/Vulkan/VulkanContext.h"
#include "Vultron/Vulkan/VulkanImage.h"
#include "Vultron/Vulkan/VulkanMesh.h"

namespace Vultron
{
    class VulkanEnvironmentMap
    {
    private:
        // We use cubemaps instead of SHs for now
        // VulkanBuffer m_irradianceVolumeBuffer;
        VulkanImage m_irradiance;
        VolumeData m_volume;
        VulkanImage m_prefiltered;
        VulkanBuffer m_probeBuffer;

        VkDescriptorSet m_environmentSet;
        VkDescriptorSet m_skyboxSet;

        bool InitializeDescriptorSets(const VulkanContext &context, VkDescriptorPool descriptorPool, VkDescriptorSetLayout environmentLayout, VkSampler sampler);

    public:
        VulkanEnvironmentMap(const VulkanImage &irradiance, const VolumeData &volume, const VulkanImage &prefiltered, const VulkanBuffer &probeBuffer)
            : m_irradiance(irradiance), m_volume(volume), m_prefiltered(prefiltered), m_probeBuffer(probeBuffer)
        {
        }

        VulkanEnvironmentMap() = default;
        ~VulkanEnvironmentMap() = default;

        struct EnvironmentMapCreateInfo
        {
            const std::string &irradianceFilepath;
            const std::string &prefilteredFilepath;
            const VolumeData &irradianceVolumeData;
            const std::vector<glm::vec3> &probePositions;
            ImageTransitionQueue *imageTransitionQueue;
        };

        static VulkanImage GenerateIrradianceMap(const VulkanContext &context, VkCommandPool commandPool, VkDescriptorPool descriptorPool, const VulkanMesh &skyboxMesh, const VulkanImage &environmentMap);
        static std::vector<SHData> GenerateIrradianceSHs(const VulkanContext &context, VkCommandPool commandPool, VkDescriptorPool descriptorPool, const VulkanMesh &skyboxMesh, const VulkanImage &environmentMapArray);
        static VulkanImage GenerateCubemapFromSHs(const VulkanContext &context, VkCommandPool commandPool, VkDescriptorPool descriptorPool, const VulkanMesh &skyboxMesh, const std::vector<SHData> &shData);
        static VulkanImage GeneratePrefilteredMap(const VulkanContext &context, VkCommandPool commandPool, VkDescriptorPool descriptorPool, const VulkanMesh &skyboxMesh, const VulkanImage &environmentMap);

        static VulkanEnvironmentMap CreateFromFile(const VulkanContext &context, VkCommandPool commandPool, VkDescriptorPool descriptorPool, VkDescriptorSetLayout environmentLayout, VkSampler sampler, const EnvironmentMapCreateInfo &info);
        void Destroy(const VulkanContext &context);

        // const VulkanBuffer &GetIrradianceVolumeBuffer() const { return m_irradianceVolumeBuffer; }
        const VulkanImage &GetIrradiance() const { return m_irradiance; }
        const VolumeData &GetIrradianceVolume() const { return m_volume; }
        const VulkanImage &GetPrefiltered() const { return m_prefiltered; }

        const VkDescriptorSet &GetEnvironmentDescriptorSet() const { return m_environmentSet; }
    };
}
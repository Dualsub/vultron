#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <vector>
#include <string>

namespace Vultron
{
    struct AnimationFrame
    {
        glm::vec3 position;
        float _padding0;
        glm::quat rotation;
        glm::vec3 scale;
        float _padding1;
    };

    class VulkanAnimation
    {
    private:
        uint32_t m_animationFrameOffset = 0;
        uint32_t m_animationFrameCount = 0;
        std::vector<float> m_times;

    public:
        VulkanAnimation(uint32_t frameOffset, uint32_t frameCount, const std::vector<float> &times)
            : m_animationFrameOffset(frameOffset), m_animationFrameCount(frameCount), m_times(times)
        {
        }
        VulkanAnimation() = default;
        ~VulkanAnimation() = default;

        struct AnimationFromFileCreateInfo
        {
            const std::string &filepath;
        };

        static VulkanAnimation CreateFromFile(std::vector<AnimationFrame> &frames, const AnimationFromFileCreateInfo &createInfo);

        uint32_t GetFrameOffset() const { return m_animationFrameOffset; }
        uint32_t GetFrameCount() const { return m_animationFrameCount; }
        const std::vector<float> &GetTimes() const { return m_times; }
    };
}
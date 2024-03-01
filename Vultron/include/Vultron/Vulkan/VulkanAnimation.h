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

    public:
        VulkanAnimation(uint32_t frameOffset, uint32_t frameCount)
            : m_animationFrameOffset(frameOffset), m_animationFrameCount(frameCount)
        {
        }
        VulkanAnimation() = default;
        ~VulkanAnimation() = default;

        struct AnimationFromFileCreateInfo
        {
            const std::string &filepath;
        };

        static VulkanAnimation CreateFromFile(std::vector<AnimationFrame> &frames, const AnimationFromFileCreateInfo &createInfo);
    };
}
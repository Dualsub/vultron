#include "Vultron/Vulkan/VulkanAnimation.h"

#include <fstream>

namespace Vultron
{
    VulkanAnimation VulkanAnimation::CreateFromFile(std::vector<AnimationFrame> &frames, const VulkanAnimation::AnimationFromFileCreateInfo &createInfo)
    {
        std::vector<AnimationFrame> animationFrames;
        std::vector<float> times;

        std::ifstream file(createInfo.filepath, std::ios::ate | std::ios::binary);

        assert(file.is_open() && "Failed to open file");

        file.seekg(0);

        // Read the file from the end to get the size
        uint32_t frameCount = 0;
        file.read(reinterpret_cast<char *>(&frameCount), sizeof(frameCount));

        uint32_t boneCount = 0;
        file.read(reinterpret_cast<char *>(&boneCount), sizeof(boneCount));

        animationFrames.resize(frameCount * boneCount);
        file.read(reinterpret_cast<char *>(animationFrames.data()), animationFrames.size() * sizeof(AnimationFrame));

        times.resize(frameCount);
        file.read(reinterpret_cast<char *>(times.data()), times.size() * sizeof(float));

        file.close();

        uint32_t frameOffset = static_cast<uint32_t>(frames.size());
        frames.insert(frames.end(), animationFrames.begin(), animationFrames.end());

        return VulkanAnimation(frameOffset, frameCount, times);
    }
}
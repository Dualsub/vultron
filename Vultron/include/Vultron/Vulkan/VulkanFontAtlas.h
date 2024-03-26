#pragma once

#include "Vultron/Vulkan/VulkanImage.h"

#include <glm/glm.hpp>

#include <unordered_map>

namespace Vultron
{
    struct FontGlyph
    {
        char character;
        glm::vec2 uvOffset;
        glm::vec2 uvExtent;
        float aspectRatio;
    };

    class VulkanFontAtlas
    {
    private:
        VulkanImage m_atlas;
        std::unordered_map<char, FontGlyph> m_glyphs;

    public:
        VulkanFontAtlas(const VulkanImage &atlas, const std::unordered_map<char, FontGlyph> &glyphs)
            : m_atlas(atlas), m_glyphs(glyphs) {}
        VulkanFontAtlas() = default;
        ~VulkanFontAtlas() = default;

        struct FontAtlasFromFileCreateInfo
        {
            const std::string &filepath;
            VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
        };

        static VulkanFontAtlas CreateFromFile(const VulkanContext &context, VkCommandPool commandPool, const FontAtlasFromFileCreateInfo &createInfo);

        FontGlyph GetGlyph(char character) const { return m_glyphs.at(character); }
        const VulkanImage GetAtlas() const { return m_atlas; }

        void Destroy(const VulkanContext &context);
    };
}
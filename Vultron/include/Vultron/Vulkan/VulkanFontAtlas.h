#pragma once

#include "Vultron/Vulkan/VulkanImage.h"

#include <glm/glm.hpp>

#include <string>
#include <unordered_map>

namespace Vultron
{

    struct FontGlyph
    {
        std::string character;
        glm::vec2 uvOffset;
        glm::vec2 uvExtent;
        float aspectRatio;
    };

    using GlyphMap = std::unordered_map<std::string, FontGlyph>;

    class VulkanFontAtlas
    {
    private:
        VulkanImage m_atlas;
        GlyphMap m_glyphs;

    public:
        VulkanFontAtlas(const VulkanImage &atlas, const GlyphMap &glyphs)
            : m_atlas(atlas), m_glyphs(glyphs) {}
        VulkanFontAtlas() = default;
        ~VulkanFontAtlas() = default;

        struct FontAtlasFromFileCreateInfo
        {
            const std::string &filepath;
            VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
        };

        static VulkanFontAtlas CreateFromFile(const VulkanContext &context, VkCommandPool commandPool, const FontAtlasFromFileCreateInfo &createInfo);

        FontGlyph GetGlyph(const std::string &name) const { return m_glyphs.at(name); }
        const VulkanImage GetAtlas() const { return m_atlas; }

        void Destroy(const VulkanContext &context);
    };
}
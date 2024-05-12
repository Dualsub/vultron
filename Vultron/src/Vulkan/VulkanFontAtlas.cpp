#include "Vultron/Vulkan/VulkanFontAtlas.h"

#include <fstream>
#include <iostream>

namespace Vultron
{
    VulkanFontAtlas VulkanFontAtlas::CreateFromFile(const VulkanContext &context, VkCommandPool commandPool, const FontAtlasFromFileCreateInfo &createInfo)
    {
        std::fstream file(createInfo.filepath, std::ios::in | std::ios::binary);
        if (!file.is_open())
        {
            std::cerr << "Failed to open file: " << createInfo.filepath << std::endl;
        }

        assert(file.is_open() && "Failed to open file");

        struct Header
        {
            uint32_t width;
            uint32_t height;
            uint32_t numChannels;
            uint32_t numBytesPerChannel;
            uint32_t numMipLevels;
        } header;

        file.seekg(0);

        file.read(reinterpret_cast<char *>(&header), sizeof(header));

        std::vector<MipInfo> mips = VulkanImage::ReadMipsFromFile(file, header.width, header.height, header.numMipLevels, header.numChannels, header.numBytesPerChannel);
        std::vector<std::vector<MipInfo>> layers;
        layers.push_back(std::move(mips));

        VulkanImage image = VulkanImage::Create(
            {.device = context.GetDevice(),
             .commandPool = commandPool,
             .queue = context.GetGraphicsQueue(),
             .allocator = context.GetAllocator(),
             .info = {
                 .width = static_cast<uint32_t>(header.width),
                 .height = static_cast<uint32_t>(header.height),
                 .mipLevels = static_cast<uint32_t>(header.numMipLevels),
                 .format = createInfo.format}});

        image.UploadData(context.GetDevice(), context.GetAllocator(), commandPool, context.GetGraphicsQueue(), header.numChannels * header.numBytesPerChannel, layers);

        GlyphMap glpyhs;

        uint32_t numGlyphs;
        file.read(reinterpret_cast<char *>(&numGlyphs), sizeof(numGlyphs));

        for (uint32_t i = 0; i < numGlyphs; i++)
        {
            FontGlyph glyph;

            // Read string length and then the string
            uint32_t length;
            file.read(reinterpret_cast<char *>(&length), sizeof(length));
            std::string character;
            character.resize(length);
            file.read(character.data(), length);
            glyph.character = character;

            file.read(reinterpret_cast<char *>(&glyph.uvOffset), sizeof(glyph.uvOffset));
            file.read(reinterpret_cast<char *>(&glyph.uvExtent), sizeof(glyph.uvExtent));
            file.read(reinterpret_cast<char *>(&glyph.aspectRatio), sizeof(glyph.aspectRatio));

            glpyhs[glyph.character] = glyph;
        }

        file.close();

        return VulkanFontAtlas(image, glpyhs);
    }

    void VulkanFontAtlas::Destroy(const VulkanContext &context)
    {
        m_atlas.Destroy(context);
        m_glyphs.clear();
    }
}
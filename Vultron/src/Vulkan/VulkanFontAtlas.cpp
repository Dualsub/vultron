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
            uint32_t numChannels;
            uint32_t numBytesPerChannel;
            uint32_t numMipLevels;
        } header;

        file.seekg(0);

        file.read(reinterpret_cast<char *>(&header), sizeof(header));

        std::vector<MipInfo> mips = VulkanImage::ReadMipsFromFile(file, header.numMipLevels, header.numChannels, header.numBytesPerChannel);

        VulkanImage image = VulkanImage::Create(
            {.device = context.GetDevice(),
             .commandPool = commandPool,
             .queue = context.GetGraphicsQueue(),
             .allocator = context.GetAllocator(),
             .info = {
                 .width = static_cast<uint32_t>(mips[0].width),
                 .height = static_cast<uint32_t>(mips[0].height),
                 .depth = 1,
                 .mipLevels = static_cast<uint32_t>(mips.size()),
                 .format = createInfo.format}});

        image.UploadData(context.GetDevice(), context.GetAllocator(), commandPool, context.GetGraphicsQueue(), mips);

        std::unordered_map<char, FontGlyph> glpyhs;

        uint32_t numGlyphs;
        file.read(reinterpret_cast<char *>(&numGlyphs), sizeof(numGlyphs));

        for (uint32_t i = 0; i < numGlyphs; i++)
        {
            FontGlyph glyph;
            file.read(reinterpret_cast<char *>(&glyph.character), sizeof(glyph.character));
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
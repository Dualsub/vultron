import argparse
from PIL import Image, ImageDraw, ImageFont
from dataclasses import dataclass
from typing import Tuple, Dict
import struct
from pack_image import generate_mipmaps
import numpy as np

@dataclass
class FontInfo:
    char: str
    uvPos: Tuple[int, int]
    uvSize: Tuple[int, int]
    aspectRatio: float = 1.0

def get_char_width(font, char):
    (width, baseline), (offset_x, offset_y) = font.font.getsize(char)
    return width - offset_x * 4

def create_image(font_path, font_size, image_height, custom_glyphs: Dict[str, Image] = {}):
    # Load the font with the specified size
    font = ImageFont.truetype(font_path, font_size)

    ascent, descent = font.getmetrics()

    font_height = ascent + descent
    print("font_height: ", font_height)

    # Resize all custom glyphs to the font height
    for char, glyph in custom_glyphs.items():
        custom_glyphs[char] = glyph.resize((int(glyph.width * font_height / glyph.height), font_height), Image.LANCZOS)

    # Draw ASCII characters
    chars = [chr(i) for i in range(32, 127)]
    char_widths = [get_char_width(font, char) for char in chars] + [glyph.width for glyph in custom_glyphs.values()]
    chars = chars + list(custom_glyphs.keys())

    print(list(zip(chars, char_widths)))

    # Create an image canvas. Adjust the size as needed
    image_size = (sum(char_widths), font_height)
    image = Image.new('RGBA', image_size, color=(0, 0, 0, 0))

    # Create a drawing context
    draw = ImageDraw.Draw(image)

    # Position for the first character
    start_position = 0

    font_info = []
    for char, char_width in zip(chars, char_widths):
        if char not in custom_glyphs:
            # Get offset for the character
            (width, baseline), (offset_x, offset_y) = font.font.getsize(char)
            draw.text((start_position - offset_x * 4, 0), char, fill=(255, 255, 255, 255), font=font)
        else:
            glyph_image = custom_glyphs[char]
            glyph = glyph_image.convert("RGBA")
            # Resize the glyph to the font height
            image.paste(glyph, (start_position, 0))

        # draw.rectangle([start_position, 0, start_position + char_width, font_height], outline=(255, 255, 255, 255))
        
        font_info.append(FontInfo(char, (start_position / image_size[0], 0), (char_width / image_size[0], font_height / image_size[1]), char_width / font_height))
        start_position += char_width

    image.save("font_atlas.png")
    # image = image.resize((image_height * image.width // image.height, image_height), Image.LANCZOS)

    return image, font_info

def pack_font_atlas(image, font_info, numMipLevels, output_path):
    with open(output_path, "wb") as f:
        mipmaps = generate_mipmaps(np.array(image), numMipLevels)
        f.write(struct.pack("I", image.width))
        f.write(struct.pack("I", image.height))
        f.write(struct.pack("I", 4))
        f.write(struct.pack("I", 1))
        f.write(struct.pack("I", len(mipmaps)))
        for mipmap in mipmaps:
            f.write(mipmap.tobytes())

        f.write(struct.pack("I", len(font_info)))
        for info in font_info:
            f.write(struct.pack("I", len(info.char)))
            # Write the characters as bytes, without padding or null terminator
            f.write(info.char.encode("ascii"))
            f.write(struct.pack("ff", *info.uvPos))
            f.write(struct.pack("ff", *info.uvSize))
            f.write(struct.pack("f", info.aspectRatio))

def main():
    parser = argparse.ArgumentParser(description="Generate an image of ASCII characters in a specified font.")
    parser.add_argument("input", type=str, help="Path to the font file.")
    parser.add_argument("-o", "--output", type=str, help="Path to save the output image.")
    parser.add_argument("--font-size", type=int, default=200, help="Font size for the characters.")
    parser.add_argument("--image-height", type=int, default=256, help="Height of the output image.")
    parser.add_argument("--mips", type=int, default=-1, help="Number of mipmaps to generate.")
    parser.add_argument("--custom-glyphs", type=str, help="Path to a file containing custom glyphs.")
    args = parser.parse_args()

    # Load custom glyphs
    custom_glyphs = {}
    if args.custom_glyphs:
        with open(args.custom_glyphs, "r") as f:
            for line in f:
                char, path = line.strip().split(" ")
                custom_glyphs[char] = Image.open(path)

    # Load image
    image, font_info = create_image(args.input, args.font_size, args.image_height, custom_glyphs)

    mipmapLevels = args.mips
    if mipmapLevels == -1:
        mipmapLevels = int(
            np.floor(np.log2(min(image.width, image.height))) + 1)

    pack_font_atlas(image, font_info, mipmapLevels, args.output)


if __name__ == "__main__":
    main()
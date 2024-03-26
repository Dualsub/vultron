import argparse
from PIL import Image, ImageDraw, ImageFont
from dataclasses import dataclass
from typing import Tuple
import struct
from pack_image import generate_mipmaps
import numpy as np

@dataclass
class FontInfo:
    char: str
    uvPos: Tuple[int, int]
    uvSize: Tuple[int, int]

def get_char_width(font, char):
    (width, baseline), (offset_x, offset_y) = font.font.getsize(char)
    return width + offset_x

def create_image(font_path, font_size):
    # Load the font with the specified size
    font = ImageFont.truetype(font_path, font_size)

    ascent, descent = font.getmetrics()

    font_height = ascent + descent
    print("font_height: ", font_height)

    # Draw ASCII characters A-Z and 0-9
    chars = [chr(i) for i in range(65, 91)] + [chr(i) for i in range(48, 58)]
    char_widths = [get_char_width(font, char) for char in chars]

    # Create an image canvas. Adjust the size as needed
    image_size = (sum(char_widths), font_height)
    image = Image.new('RGBA', image_size, color=(0, 0, 0, 0))

    # Create a drawing context
    draw = ImageDraw.Draw(image)

    # Position for the first character
    start_position = 0

    font_info = []
    for char, char_width in zip(chars, char_widths):
        draw.text((start_position, 0), char, fill=(255, 255, 255, 255), font=font)
        # draw.rectangle([start_position, 0, start_position + char_width, font_height], outline=(255, 255, 255, 255))
        
        font_info.append(FontInfo(char, (start_position / image_size[0], 0), (char_width / image_size[0], font_height / image_size[1])))
        start_position += char_width

    return image, font_info

def pack_font_atlas(image, font_info, numMipLevels, output_path):
    with open(output_path, "wb") as f:
        mipmaps = generate_mipmaps(image, numMipLevels)
        f.write(struct.pack("I", 4))
        f.write(struct.pack("I", 1))
        f.write(struct.pack("I", len(mipmaps)))
        for mipmap in mipmaps:
            f.write(struct.pack("I", mipmap.width))
            f.write(struct.pack("I", mipmap.height))
            f.write(mipmap.tobytes())

        f.write(struct.pack("I", len(font_info)))
        for info in font_info:
            f.write(struct.pack("c", info.char.encode("ascii")))
            f.write(struct.pack("ff", *info.uvPos))
            f.write(struct.pack("ff", *info.uvSize))



def main():
    parser = argparse.ArgumentParser(description="Generate an image of ASCII characters in a specified font.")
    parser.add_argument("input", type=str, help="Path to the font file.")
    parser.add_argument("-o", "--output", type=str, help="Path to save the output image.")
    parser.add_argument("--font-size", type=int, default=200, help="Font size for the characters.")
    parser.add_argument("--mips", type=int, default=-1, help="Number of mipmaps to generate.")
    args = parser.parse_args()

    image, font_info = create_image(args.input, args.font_size)

    mipmapLevels = args.mips
    if mipmapLevels == -1:
        mipmapLevels = int(
            np.floor(np.log2(max(image.width, image.height)))) + 1

    pack_font_atlas(image, font_info, mipmapLevels, args.output)


if __name__ == "__main__":
    main()
import argparse
import struct
import numpy as np
from PIL import Image
# Take a image file, generate mipmaps and pack it into a vultron image file


def generate_mipmaps(image, levels):
    mipmaps = [image]
    for _ in range(1, levels):
        mipmaps.append(mipmaps[-1].resize((max(1, mipmaps[-1].width // 2),
                       max(1, mipmaps[-1].height // 2)), Image.LANCZOS))

    return mipmaps


def main():
    parser = argparse.ArgumentParser(
        description="Pack a image file into a vultron image file")
    # pack_image.py inputimage.png -o outputimage.bin
    parser.add_argument("input", help="The input image file")
    parser.add_argument(
        "-o", "--output", help="The output vultron image file", default="image.bin")
    parser.add_argument(
        "-l", "--levels", help="The number of mipmap levels to generate", default=-1, type=int)
    args = parser.parse_args()

    image = Image.open(args.input)
    if image.mode == "RGB":
        image = image.convert("RGBA")

    assert args.input != args.output, "Input and output file cannot be the same"

    mipmapLevels = args.levels
    if mipmapLevels == -1:
        mipmapLevels = int(
            np.floor(np.log2(max(image.width, image.height)))) + 1

    numChannels = 0
    if image.mode == "RGBA":
        numChannels = 4
    elif image.mode == "RGB":
        numChannels = 3

    assert numChannels > 0, "Unsupported image mode"

    mipmaps = generate_mipmaps(image, mipmapLevels)
    with open(args.output, "wb") as f:
        f.write(struct.pack("I", numChannels))
        f.write(struct.pack("I", 1))
        f.write(struct.pack("I", len(mipmaps)))
        for mipmap in mipmaps:
            # Save width, height, channels, and the number of bytes in the datatype used, i.e. 1 for uint8
            f.write(struct.pack("I", mipmap.width))
            f.write(struct.pack("I", mipmap.height))
            f.write(mipmap.tobytes())

    print(f"Image packed into {args.output}")
    print(f"Generated {len(mipmaps)} mipmaps")


if __name__ == "__main__":
    main()

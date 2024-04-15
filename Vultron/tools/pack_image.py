import argparse
import struct
import numpy as np
from PIL import Image
# Take a image file, generate mipmaps and pack it into a vultron image file


def generate_mipmaps(image, mips):
    mipmaps = [image]
    for _ in range(1, mips):
        mipmaps.append(mipmaps[-1].resize((max(1, mipmaps[-1].width // 2),
                       max(1, mipmaps[-1].height // 2)), Image.LANCZOS))

    return mipmaps

class ImageTypes:
    Texture2D = 0
    Cubemap = 1

def main():
    parser = argparse.ArgumentParser(
        description="Pack a image file into a vultron image file")
    # pack_image.py inputimage0.png inputimage1.png -o outputimage.bin
    parser.add_argument("input", nargs="+", help="The input image file")
    parser.add_argument(
        "-o", "--output", help="The output vultron image file", default="image.bin")
    parser.add_argument(
        "-m", "--mips", help="The number of mipmap levels to generate", default=-1, type=int)
    parser.add_argument(
        "-f", "--flip", help="Flip the image vertically", action="store_true")
    parser.add_argument(
        "-i", "--invert", help="Invert the image", action="store_true")
    parser.add_argument(
        "-c", "--cubemap", help="Pack the image as a cubemap", action="store_true")

    args = parser.parse_args()


    inputs = [*args.input]
    
    print("Using input(s):", end=" ")
    for input in inputs:
        print(input, end=" ")
    print()
    
    imageType = ImageTypes.Texture2D
    if args.cubemap:
        print("Packing as a cubemap")
        imageType = ImageTypes.Cubemap

    layers = []
    for input in inputs:
        image = Image.open(input)
        if image.mode == "RGB" or image.mode == "P" or image.mode == "L":
            image = image.convert("RGBA")

        if args.invert:
            print("Inverting image")
            image = Image.eval(image, lambda x: 255 - x)

        if args.flip:
            print("Flipping image vertically")
            image = image.transpose(Image.FLIP_TOP_BOTTOM)

        assert args.input != args.output, "Input and output file cannot be the same"

        mipmapLevels = args.mips
        if mipmapLevels == -1:
            mipmapLevels = int(
                np.floor(np.log2(max(image.width, image.height)))) + 1

        numChannels = 0
        if image.mode == "RGBA":
            numChannels = 4
        elif image.mode == "RGB":
            numChannels = 3

        assert numChannels > 0, "Unsupported image mode: " + image.mode
        mipmaps = generate_mipmaps(image, mipmapLevels)
        layers.append(mipmaps)

    # Assert that all images have the same dimensions and same number of mipmaps
    for i in range(1, len(layers)):
        assert len(layers[i]) == len(layers[0]), "All images must have the same number of mipmaps"
        for j in range(len(layers[i])):
            assert layers[i][j].width == layers[0][j].width, "All images must have the same width"
            assert layers[i][j].height == layers[0][j].height, "All images must have the same height"

    with open(args.output, "wb") as f:
        f.write(struct.pack("I", layers[0][0].width))
        f.write(struct.pack("I", layers[0][0].height))
        f.write(struct.pack("I", numChannels))
        f.write(struct.pack("I", 1))
        f.write(struct.pack("I", len(layers)))
        f.write(struct.pack("I", len(layers[0])))
        f.write(struct.pack("I", imageType))
        for mipmaps in layers:
            for mipmap in mipmaps:
                # Save width, height, channels, and the number of bytes in the datatype used, i.e. 1 for uint8
                f.write(mipmap.tobytes())

    print(f"Image packed into {args.output}")
    print(f"Generated {len(mipmaps)} mipmaps")


if __name__ == "__main__":
    main()
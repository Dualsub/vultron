import argparse
import struct
import numpy as np
import cv2 as cv
# Take a image file, generate mipmaps and pack it into a vultron image file


def generate_mipmaps(image, mips):
    mipmaps = [image]
    for _ in range(1, mips):
        mipmaps.append(cv.resize(mipmaps[-1], (mipmaps[-1].shape[1] // 2, mipmaps[-1].shape[0] // 2), interpolation=cv.INTER_LANCZOS4))

    return mipmaps

FORMATS = {
    ".hdr": "HDR-FI",
    ".png": "PNG-FI",
}

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
        image = cv.imread(input, cv.IMREAD_UNCHANGED)
        image = cv.cvtColor(image, cv.COLOR_BGR2RGB)
        height, width, channels = image.shape

        print(f"Loaded image with dimensions {width}x{height} and {channels} channels, type: {image.dtype}")
        
        if channels == 3:
            image = cv.cvtColor(image, cv.COLOR_RGB2RGBA)
            channels = 4

        if args.invert:
            print("Inverting image")
            image = 1 - image

        if args.flip:
            print("Flipping image vertically")
            image = cv.flip(image, 0)

        assert args.input != args.output, "Input and output file cannot be the same"

        mipmapLevels = args.mips
        if mipmapLevels == -1:
            mipmapLevels = int(
                np.floor(np.log2(max(width, height)))) + 1

        assert channels > 0, "Unsupported image format: 0 channels"
        mipmaps = generate_mipmaps(image, mipmapLevels)
        layers.append(mipmaps)

    # Assert that all images have the same dimensions and same number of mipmaps
    for i in range(1, len(layers)):
        assert len(layers[i]) == len(layers[0]), "All images must have the same number of mipmaps"
        for j in range(len(layers[i])):
            assert layers[i][j].shape == layers[0][j].shape, "All images must have the same dimensions"
            
    with open(args.output, "wb") as f:
        f.write(struct.pack("I", layers[0][0].shape[1]))
        f.write(struct.pack("I", layers[0][0].shape[0]))
        f.write(struct.pack("I", channels))
        f.write(struct.pack("I", np.dtype(image.dtype).itemsize))
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

# python pack_image.py C:\dev\repos\vultron\Vultron\assets\textures\loft\px.hdr C:\dev\repos\vultron\Vultron\assets\textures\loft\nx.hdr C:\dev\repos\vultron\Vultron\assets\textures\loft\py.hdr C:\dev\repos\vultron\Vultron\assets\textures\loft\ny.hdr C:\dev\repos\vultron\Vultron\assets\textures\loft\pz.hdr C:\dev\repos\vultron\Vultron\assets\textures\loft\nz.hdr -o C:\dev\repos\vultron\Vultron\assets\textures\skybox.dat -c
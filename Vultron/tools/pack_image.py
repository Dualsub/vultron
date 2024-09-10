import argparse
from io import BufferedWriter
import struct
import numpy as np
import cv2 as cv
from PIL import Image
from skimage import transform
# Take a image file, generate mipmaps and pack it into a vultron image file

def resize_pil(image, size):
    return np.array(Image.fromarray(image).resize(size, Image.LANCZOS))

def resize_opencv(image, size):
    return cv.resize(image, size, interpolation=cv.INTER_LANCZOS4)

def resize_skimage(image, size):
    return transform.resize(image, size, anti_aliasing=True)

def generate_mipmaps(image, mips, resize_func):
    mipmaps = [image] 
    for _ in range(1, mips):
        mip_width = mipmaps[-1].shape[1] // 2
        mip_height = mipmaps[-1].shape[0] // 2
        mipmaps.append(resize_func(mipmaps[-1], (mip_width, mip_height)))

    return mipmaps

FORMATS = {
    ".hdr": "HDR-FI",
    ".png": "PNG-FI",
}

class ImageTypes:
    Texture2D = 0
    Cubemap = 1

def pack_image(image_files, output_file, resize = None, mips = -1, flip = False, flip_horizontal = False, invert = False, cubemap = False):
    
    inputs = [*image_files]
    
    print("Using input(s):", end=" ")
    for input in inputs:
        print(input, end=" ")
    print()
    
    imageType = ImageTypes.Texture2D
    if cubemap:
        print("Packing as a cubemap")
        imageType = ImageTypes.Cubemap
    layers = []

    for i, input in enumerate(inputs):

        try:
            image = cv.imread(input, cv.IMREAD_UNCHANGED)
            if image.shape[2] == 4:
                image = cv.cvtColor(image, cv.COLOR_BGRA2RGBA)
            else:
                image = cv.cvtColor(image, cv.COLOR_BGR2RGB)
        except Exception:
            image = np.array(Image.open(input))

        resize_func = resize_pil if image.dtype == np.uint8 else resize_skimage

        height, width, channels = image.shape
        if resize:
            print(f"Resizing image to {resize[0]}x{resize[1]}")
            image = resize_func(image, resize)
            height, width, channels = image.shape

        print(f"Loaded image with dimensions {width}x{height} and {channels} channels, type: {image.dtype}")
        
        if channels == 3:
            image = cv.cvtColor(image, cv.COLOR_RGB2RGBA)
            channels = 4

        if invert:
            print("Inverting image")
            image = 1 - image

        if (cubemap and (i == 2 or i == 3)):
            print("Flipping image horizontally")
            image = cv.flip(image, 1)
            image = cv.flip(image, 0)

        if flip:
            print("Flipping image vertically")
            image = cv.flip(image, 0)

        if flip_horizontal:
            print("Flipping image horizontally")
            image = cv.flip(image, 1)

        assert input != output_file, "Input and output file cannot be the same"

        mipmapLevels = mips
        if mipmapLevels == -1:
            mipmapLevels = int(
                np.floor(np.log2(min(width, height))) + 1)

        assert channels > 0, "Unsupported image format: 0 channels"
        mipmaps = generate_mipmaps(image, mipmapLevels, resize_func)
        layers.append(mipmaps)

    # Assert that all images have the same dimensions and same number of mipmaps
    for i in range(1, len(layers)):
        assert len(layers[i]) == len(layers[0]), "All images must have the same number of mipmaps"
        for j in range(len(layers[i])):
            assert layers[i][j].shape == layers[0][j].shape, "All images must have the same dimensions"
            
    with open(output_file, "wb") as f:
        write_layers(f, layers, imageType)

    print(f"Image packed into {output_file}")
    print(f"Generated {len(layers)} layers")
    print(f"Generated {len(mipmaps)} mipmaps")

def write_layers(f: BufferedWriter, layers, imageType: int):
    f.write(struct.pack("I", layers[0][0].shape[1]))
    f.write(struct.pack("I", layers[0][0].shape[0]))
    f.write(struct.pack("I", layers[0][0].shape[2]))
    f.write(struct.pack("I", np.dtype(layers[0][0].dtype).itemsize))
    f.write(struct.pack("I", len(layers)))
    f.write(struct.pack("I", len(layers[0])))
    f.write(struct.pack("I", imageType))
    for mipmaps in layers:
        for mipmap in mipmaps:
            f.write(mipmap.tobytes())

def pack_all(dir):
    # dir = "C:/dev/repos/arcane-siege/assets"

    # Take all png, jpg, jpeg, and tga files in the directory and pack them into a vultron image file, it should include files from subdirectories
    import os
    files = []
    for root, _, filenames in os.walk(dir):
        for filename in filenames:
            if filename.lower().endswith((".png", ".jpg", ".jpeg")):
                files.append(os.path.join(root, filename))

    print("Packing", len(files), "images")
    print("Files:", files)

    failed_files = []
    if "y" == input("Proceed? [y/n] ").lower():
        for file in files:
            file_without_ext, _ = os.path.splitext(file)
            try:
                pack_image([file], file_without_ext + ".dat", -1, False, False, False)
            except Exception as e:
                failed_files.append((file, e))


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
        "-r", "--resize", help="Resize the image to the specified dimensions", nargs=2, type=int)
    parser.add_argument(
        "-f", "--flip", help="Flip the image vertically", action="store_true")
    parser.add_argument(
        "-fh", "--flip_horizontal", help="Flip the image horizontally", action="store_true")
    parser.add_argument(
        "-i", "--invert", help="Invert the image", action="store_true")
    parser.add_argument(
        "-c", "--cubemap", help="Pack the image as a cubemap", action="store_true")
    parser.add_argument(
        "--all", help="Pack all images in a directory", action="store_true")

    args = parser.parse_args()

    if args.all:
        pack_all(args.input[0])
    elif args.cubemap:
        sides = ["px", "nx", "py", "ny", "pz", "nz"]
        base_path = args.input[0]
        paths = [base_path.replace("*", side) for side in sides]
        pack_image(paths, args.output, args.resize, args.mips, args.flip, args.flip_horizontal, args.invert, args.cubemap)
    else:
        pack_image(args.input, args.output, args.resize, args.mips, args.flip, args.flip_horizontal, args.invert, args.cubemap)

if __name__ == "__main__":
    main()

    # print("Failed for files:", failed_files)
# python pack_image.py C:/dev/repos/vultron/Vultron/assets/textures/loft/px.hdr C:/dev/repos/vultron/Vultron/assets/textures/loft/nx.hdr C:/dev/repos/vultron/Vultron/assets/textures/loft/py.hdr C:/dev/repos/vultron/Vultron/assets/textures/loft/ny.hdr C:/dev/repos/vultron/Vultron/assets/textures/loft/pz.hdr C:/dev/repos/vultron/Vultron/assets/textures/loft/nz.hdr -o C:/dev/repos/vultron/Vultron/assets/textures/skybox.dat -c
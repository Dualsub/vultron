import argparse
import os
from PIL import Image
import numpy as np
from pack_image import generate_mipmaps, write_layers, ImageTypes
import struct

def main():
    parser = argparse.ArgumentParser(description='Pack particle atlas')
    parser.add_argument("input", nargs="+", help="key=image.png")
    parser.add_argument(
        "-o", "--output", help="The output vultron image file", default="atlas.dat")
    parser.add_argument("-s", "--size", help="Size of the atlas", default=2048, type=int)
    parser.add_argument("--show", help="Show the atlas", action="store_true")
    parser.add_argument("--minimize", help="Minimize the atlas", action="store_true")
    args = parser.parse_args()

    images = {}
    for input in args.input:
        key, file = input.split("=")
        image = Image.open(file)
        if image.mode != "RGBA":
            image = image.convert("RGBA")
        if image.size[0] != args.size or image.size[1] != args.size:
            image = image.resize((args.size, args.size), Image.LANCZOS)
        images[key] = image

    print(f"Packing {len(images)} images into {args.output}")

    atlas_width = 0
    atlas_height = 0
    for image in images.values():
        atlas_width += image.width
        atlas_height = max(atlas_height, image.height)

    if not args.minimize:
        atlas_height = atlas_width = max(atlas_width, atlas_height)

    atlas = Image.new("RGBA", (atlas_width, atlas_height), (0, 0, 0, 0))

    regions = {}
    x = 0
    y = 0
    for key, image in images.items():
        atlas.paste(image, (x, y))
        x += image.width

        # In UV coordinates
        regions[key] = (x / atlas_width, y / atlas_height,
                        image.width / atlas_width, image.height / atlas_height)

        if x >= atlas_width:
            x = 0
            y += image.height

    mipmapLevels = int(
                np.floor(np.log2(min(atlas_width, atlas_height))) + 1)
    mipmaps = generate_mipmaps(np.array(atlas), mipmapLevels)

    if args.output.lower().endswith(".dat"):
        with open(args.output, "wb") as f:
            write_layers(f, [mipmaps], ImageTypes.Texture2D)
    else:
        atlas.save(args.output)




if __name__ == '__main__':
    main()
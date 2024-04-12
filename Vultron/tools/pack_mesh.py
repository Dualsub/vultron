import argparse
import struct
from pyassimp import load
import numpy as np

def pack_mesh(mesh, output_file):
    # Flipping the y and z axis
    positions = np.array(mesh.vertices)
    normals = np.array(mesh.normals)
    uvs = np.array(mesh.texturecoords[0])[:, :2]
    # Edit UVs for Vulkan
    uvs[:, 1] = 1 - uvs[:, 1]

    vertices = np.hstack((positions, normals, uvs))
    indices = np.array(mesh.faces).flatten()

    with open(output_file, "wb") as f:
        f.write(struct.pack("I", len(vertices)))
        f.write(vertices.tobytes())
        f.write(struct.pack("I", len(indices)))
        f.write(indices.tobytes())

    print(f"Mesh packed into {output_file}")

# Take a gltf or glb file and pack it into a vultron mesh file
def main():
    parser = argparse.ArgumentParser(description="Pack a gltf or glb file into a vultron mesh file")
    # pack_mesh.py inputmesh.gltf -o outputmesh.bin
    parser.add_argument("input", help="The input gltf or glb file")
    parser.add_argument("-o", "--output", help="The output vultron mesh file", default="mesh.bin")
    args = parser.parse_args()

    assert args.input != args.output, "Input and output file cannot be the same"

    with load(args.input) as scene:
        mesh = scene.meshes[0]
        pack_mesh(mesh, args.output)



if __name__ == "__main__":
    main()
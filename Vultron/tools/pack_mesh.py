import argparse
import struct
from pyassimp import load, postprocess
import pyassimp
import numpy as np

MODEL_FLAGS = (
    postprocess.aiProcess_Triangulate | 
    postprocess.aiProcess_GenSmoothNormals | postprocess.aiProcess_CalcTangentSpace)

def pack_mesh(meshes, output_file):

    vertices = np.array([], dtype=np.float32)
    indices = np.array([], dtype=np.uint32)

    print("Meshes found:", len(meshes))

    for i, mesh in enumerate(meshes):
        # Flipping the y and z axis
        positions = np.array(mesh.vertices)
        normals = np.array(mesh.normals)
        
        uvs = np.array(mesh.texturecoords[0])[:, :2]
        # Edit UVs for Vulkan
        uvs[:, 1] = 1 - uvs[:, 1]
        # Add i as 3rd UV channel
        uvs = np.hstack((uvs, np.ones((len(uvs), 1)) * i))

        new_vertices = np.hstack((positions, normals, uvs))
        new_indices = np.array(mesh.faces).flatten() + len(vertices) // 9
        
        vertices = np.append(vertices, new_vertices)
        indices = np.append(indices, new_indices)

    vertices = vertices.astype(np.float32)
    indices = indices.astype(np.uint32)

    print(f"Vertices: {len(vertices) // 9}, Indices: {len(indices)}")
    assert len(vertices.tobytes()) == 4 * len(vertices), "Vertices are not 4 byte aligned"

    with open(output_file, "wb") as f:
        f.write(struct.pack("I", len(vertices) // 9))
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

    with load(args.input, processing=MODEL_FLAGS) as scene:
        pack_mesh(scene.meshes, args.output)



if __name__ == "__main__":
    main()
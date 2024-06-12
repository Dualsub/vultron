'''
Takes a skeletal mesh in the engine format and cuts a part that contains some bones.
'''

# This is what the writer function looks like in the original script
# def write_to_binary_file(vertex_data, index_data, skeleton_data, file_path):
#     sorted_bones = sorted(skeleton_data.values(), key=lambda x: x["id"])
#     with open(file_path, "wb") as file:
#         file.write(struct.pack("I", len(vertex_data)))
#         for vertex in vertex_data:
#             for component in vertex["position"]:
#                 file.write(struct.pack("f", component))
#             for component in vertex["normals"]:
#                 file.write(struct.pack("f", component))
#             for component in vertex["texCoords"]:
#                 file.write(struct.pack("f", component))
#             for component in vertex["boneIDs"]:
#                 file.write(struct.pack("i", component))
#             for component in vertex["weights"]:
#                 file.write(struct.pack("f", component))

#         file.write(struct.pack("I", len(index_data)))
#         for index in index_data:
#             file.write(struct.pack("I", index))

#         file.write(struct.pack("I", len(sorted_bones)))
#         for bone in sorted_bones:
#             file.write(struct.pack("i", bone["id"]))
#             # -1 if the bone has no parent
#             parent_id = bone["parentId"] if bone["parentId"] is not None else -1
#             file.write(struct.pack("i", parent_id))
#             for row in bone["offsetMatrix"]:
#                 for component in row:
#                     file.write(struct.pack("f", component))

import argparse
import numpy as np
import struct

FLOAT_SIZE = 4
VERTEX_SIZE = 3 * FLOAT_SIZE + 3 * FLOAT_SIZE + 2 * FLOAT_SIZE

def main():
    parser = argparse.ArgumentParser(description='Cut a part of a skeletal mesh')
    parser.add_argument("input", help="Input skeletal mesh file")
    parser.add_argument("-o", "--output", help="Output mesh file", default="output.mesh")
    parser.add_argument("-b", "--bones", help="Bones to keep", nargs='+', type=int, default=[0])
    parser.add_argument("--re-center", help="Re-center the mesh", action="store_true")
    args = parser.parse_args()

    vertices = []
    with open(args.input, 'rb') as f:
        num_vertices = struct.unpack("I", f.read(4))[0]
        for i in range(num_vertices):
            position = struct.unpack("3f", f.read(12))
            normal = struct.unpack("3f", f.read(12))
            tex_coord = struct.unpack("2f", f.read(8))
            bone_ids = struct.unpack("4i", f.read(16))
            weights = struct.unpack("4f", f.read(16))
            vertices.append((position, normal, tex_coord, bone_ids, weights))
        
        num_indices = struct.unpack("I", f.read(4))[0]
        indices = struct.unpack(f"{num_indices}I", f.read(num_indices * 4))
        faces = np.array(indices).reshape(-1, 3)

    positions = np.array([vertex[0] for vertex in vertices], dtype=np.float32)
    normals = np.array([vertex[1] for vertex in vertices], dtype=np.float32)
    tex_coords = np.array([vertex[2] for vertex in vertices], dtype=np.float32)
    bone_ids = np.array([vertex[3] for vertex in vertices], dtype=np.int32)
    weights = np.array([vertex[4] for vertex in vertices], dtype=np.float32)

    # Find all the indices that are used by the bones we want to keep
    bone_indices = set()
    for bone_id in args.bones:
        for i, _ in enumerate(bone_ids):
            if bone_id in bone_ids[i]:
                bone_indices.add(i)

    vertex_indices = []
    for i in range(len(positions)):
        if i in bone_indices:
            vertex_indices.append(i)
    

    # Create a mapping from old vertex indices to new vertex indices
    vertex_index_map = {old_index: new_index for new_index, old_index in enumerate(vertex_indices)}
    
    new_faces = []
    for face in faces:
        new_face = [vertex_index_map[vertex_index] for vertex_index in face if vertex_index in bone_indices]
        if len(new_face) == 3:
            new_faces.append(new_face)

    new_indices = np.array(new_faces).flatten()

    # Create a new vertex buffer that only contains the vertices we want to keep
    new_positions = positions[vertex_indices]
    new_normals = normals[vertex_indices]
    new_tex_coords = tex_coords[vertex_indices]

    if args.re_center:
        new_positions -= np.mean(new_positions, axis=0)


    print(f"Original vertex count: {len(positions)}")
    print(f"New vertex count: {len(new_positions)}")
    print(f"Original index count: {len(indices)}")
    print(f"New index count: {len(new_indices)}")

    assert len(new_positions) == len(new_normals) == len(new_tex_coords)
    vertices = np.hstack((new_positions, new_normals, new_tex_coords))
    indices = np.array(new_indices)

    assert len(vertices.tobytes()) / VERTEX_SIZE == len(new_positions), f"{len(vertices.tobytes()) / VERTEX_SIZE} != {len(new_positions)}"
    assert len(indices.tobytes()) == len(new_indices) * 4

    with open(args.output, "wb") as f:
        f.write(struct.pack("I", len(vertices)))
        f.write(vertices.tobytes())
        f.write(struct.pack("I", len(indices)))
        f.write(indices.tobytes())

    print(f"Mesh cut and saved to {args.output}")

if __name__ == "__main__":
    main()

    


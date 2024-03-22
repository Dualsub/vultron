import bpy
import json
import struct

def get_vertex_data(obj):
    mesh = obj.data
    # Ensure mesh data is up-to-date
    mesh.calc_loop_triangles()
    # Access the first UV layer
    uv_layer = mesh.uv_layers.active.data if mesh.uv_layers.active is not None else None
    
    vertex_data = [None] * len(mesh.vertices)
    index_data = []

    print("Vertex count:", len(mesh.vertices))

    vertIndices = set()

    for poly in mesh.polygons:
        for idx in poly.vertices:
            index_data.append(idx)

        for idx, loop_index in enumerate(poly.loop_indices):
            loop = mesh.loops[loop_index]
            vertex = mesh.vertices[loop.vertex_index]

            if loop.vertex_index in vertIndices:
                continue
            
            vertIndices.add(loop.vertex_index)

            position = [vertex.co.x, vertex.co.y, vertex.co.z]
            normals = [vertex.normal.x, vertex.normal.y, vertex.normal.z]
            tex_coords = [uv_layer[loop_index].uv.x, uv_layer[loop_index].uv.y] if uv_layer else [0, 0]
            
            bone_ids = [-1] * 4
            weights = [0.0] * 4

            for i, group in enumerate(vertex.groups[:4]):
                if group.group < len(obj.vertex_groups):
                    bone_ids[i] = group.group
                    weights[i] = group.weight

            # Normalize weights
            total_weight = sum(weights)
            if total_weight > 0:
                weights = [w / total_weight for w in weights]

            vertex_data[loop.vertex_index] = {
                "position": position,
                "normals": normals,
                "texCoords": tex_coords,
                "boneIDs": bone_ids,
                "weights": weights
            }

    print("Vertex count:", len(vertex_data))
    print("Index count:", len(index_data))

    assert len(vertex_data) == len(mesh.vertices) and None not in vertex_data

    return vertex_data, index_data

def get_skeleton_data(armature):
    print([bone.name for bone in armature.data.bones])
    bones_data = {}
    bone_id_map = {bone.name: idx for idx, bone in enumerate(armature.data.bones)}
    for bone in armature.data.bones:
        # Get the bone's offset matrix from mesh space to bone space like Assimp
        offset_matrix = bone.matrix_local.inverted()
        bone_data = {
            "id": bone_id_map[bone.name],
            "parentId": bone_id_map[bone.parent.name] if bone.parent else None,
            "offsetMatrix": [list(row) for row in offset_matrix]
        }
        bones_data[bone.name] = bone_data
    return bones_data

def write_to_binary_file(vertex_data, index_data, skeleton_data, file_path):
    sorted_bones = sorted(skeleton_data.values(), key=lambda x: x["id"])
    with open(file_path, "wb") as file:
        file.write(struct.pack("I", len(vertex_data)))
        for vertex in vertex_data:
            for component in vertex["position"]:
                file.write(struct.pack("f", component))
            for component in vertex["normals"]:
                file.write(struct.pack("f", component))
            for component in vertex["texCoords"]:
                file.write(struct.pack("f", component))
            for component in vertex["boneIDs"]:
                file.write(struct.pack("i", component))
            for component in vertex["weights"]:
                file.write(struct.pack("f", component))

        file.write(struct.pack("I", len(index_data)))
        for index in index_data:
            file.write(struct.pack("I", index))

        file.write(struct.pack("I", len(sorted_bones)))
        for bone in sorted_bones:
            file.write(struct.pack("i", bone["id"]))
            # -1 if the bone has no parent
            parent_id = bone["parentId"] if bone["parentId"] is not None else -1
            file.write(struct.pack("i", parent_id))
            for row in bone["offsetMatrix"]:
                for component in row:
                    file.write(struct.pack("f", component))


def write_to_json_file(vertex_data, file_path):
    with open(file_path, "w") as file:
        json.dump(vertex_data, file, indent=4)

def export_selected(file_path):
    obj = bpy.context.object
    if obj.type == 'MESH':
        # Find the armature linked to the mesh
        armature = next((mod.object for mod in obj.modifiers if mod.type == 'ARMATURE'), None)
        if not armature:
            print("No armature found for the selected mesh.")
            return

        vertex_data, index_data = get_vertex_data(obj)
        skeleton_data = get_skeleton_data(armature)

        write_to_json_file(skeleton_data, file_path.replace('*', 'json'))
        write_to_binary_file(vertex_data, index_data, skeleton_data, file_path.replace('*', 'dat'))
        print("Export successful.")
    else:
        print("Selected object is not a mesh.")


# Example usage - replace 'path/to/your/output.json' with your desired path
export_selected('C:/dev/repos/arcane-siege/assets/buff/buff.*')

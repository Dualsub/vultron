import argparse
import struct
import json
from impasse import load
# from pyassimp import load, postprocess
import numpy as np
import glm

NUM_COMPONENTS = 12
DEFAULT_COMPONENTS = [
    0.0, 0.0, 0.0, 0.0,  # Position, padded
    0.0, 0.0, 0.0, 1.0,  # Quaternion
    1.0, 1.0, 1.0, 0.0  # Scale, padded
]

# Take a gltf or glb file and pack it into a vultron animation file


def main():
    parser = argparse.ArgumentParser(
        description="Pack a gltf or glb file into a vultron animation file")
    # pack_animation.py inputanimation.gltf -o outputanimation.bin
    parser.add_argument("input", help="The input gltf or glb file")
    parser.add_argument(
        "-o", "--output", help="The output vultron animation file", default="animation.bin")
    parser.add_argument(
        "-s", "--skeleton", help="The output vultron skeleton file", default="skeleton.bin")
    args = parser.parse_args()

    assert args.input != args.output, "Input and output file cannot be the same"

    skeleton_data = {}
    with open(args.skeleton) as file:
        skeleton_data = json.load(file)

    id_to_name = {bone["id"]: name for name, bone in skeleton_data.items()}

    scene = load(args.input)
    animation = scene.animations[0]
    duration = animation.duration
    ticks_per_second = animation.ticks_per_second

    frames = [None] * len(animation.channels[0].position_keys)
    for channel in animation.channels:
        # Get channel name
        node_name = channel.node_name

        for i in range(len(channel.position_keys)):
            assert channel.position_keys[i].time == channel.rotation_keys[
                i].time, "Position and rotation keys must have the same time"
            assert channel.position_keys[i].time == channel.scaling_keys[
                i].time, "Position and scaling keys must have the same time"

        # Get channel position keys
        times = []
        for position_key in channel.position_keys:
            times.append(position_key.time / ticks_per_second)

        position_keys = []
        for position_key in channel.position_keys:
            position_keys.append(
                (position_key.value[0], position_key.value[1], position_key.value[2], 0.0))

        # Get channel rotation keys
        rotation_keys = []
        for rotation_key in channel.rotation_keys:
            # Save in correct order to fit GLSL
            rotation_keys.append(
                (rotation_key.value[3], rotation_key.value[0], rotation_key.value[1], rotation_key.value[2]))

        # Get channel scaling keys
        scaling_keys = []
        for scaling_key in channel.scaling_keys:
            scaling_keys.append(
                (scaling_key.value[0], scaling_key.value[1], scaling_key.value[2], 0.0))

        # Create vector for each key
        for i in range(len(times)):
            if frames[i] is None:
                frames[i] = {"time": times[i], "bones": [
                    DEFAULT_COMPONENTS] * len(skeleton_data)}

            if node_name not in skeleton_data:
                continue

            bone_id = skeleton_data[node_name]["id"]
            frames[i]["bones"][bone_id] = [
                *position_keys[i], *rotation_keys[i], *scaling_keys[i]]
            assert len(frames[i]["bones"][bone_id]
                       ) == NUM_COMPONENTS, "Bone vector must have 12 components"

    new_frames = [*frames]
    for i, frame in enumerate(frames):
        for bone_id in range(len(frame["bones"])):
            bone_matrix = glm.mat4(1.0)
            curr_bone_id = bone_id

            while True:
                try:
                    curr_bone = frame["bones"][curr_bone_id]
                    
                    local_bone_matrix = glm.mat4(1.0)
                    
                    local_bone_matrix = glm.translate(
                        local_bone_matrix, glm.vec3(*curr_bone[0:3]))             
                    
                    local_bone_matrix = local_bone_matrix * glm.mat4_cast(
                        glm.quat(*curr_bone[4:8]))

                    local_bone_matrix = glm.scale(
                        local_bone_matrix, glm.vec3(*curr_bone[8:11]))
                    
                    bone_matrix = local_bone_matrix * bone_matrix

                    curr_bone_id = skeleton_data[id_to_name[curr_bone_id]]["parentId"]

                    if curr_bone_id is None or curr_bone_id == -1:
                        break
                except Exception as e:
                    print("Error while processing bone", curr_bone_id, skeleton_data[id_to_name[curr_bone_id]]["parentId"])
                    raise e

            new_pos = glm.vec3()
            new_rot = glm.quat()
            new_scale = glm.vec3()

            glm.decompose(bone_matrix, new_scale, new_rot, new_pos, glm.vec3(), glm.vec4())

            new_frames[i]["bones"][bone_id] = [*new_pos.to_list(), 0.0, *new_rot.to_list(), *new_scale.to_list(), 0.0]

    frames = new_frames

    with open(args.output, "wb") as file:
        file.write(struct.pack("I", len(frames)))
        file.write(struct.pack("I", len(skeleton_data)))
        print("Packing", len(frames), "frames")
        print("Packing", len(skeleton_data), "bones")
        for frame in frames:
            for i in range(len(skeleton_data)):
                for j in range(NUM_COMPONENTS):
                    file.write(struct.pack("f", frame["bones"][i][j]))

        for frame in frames:
            file.write(struct.pack("f", frame["time"]))

    print("Animation packed into", args.output)


if __name__ == "__main__":
    main()

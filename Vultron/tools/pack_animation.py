import argparse
import struct
import json
from impasse import load
# from pyassimp import load, postprocess
import numpy as np

NUM_COMPONENTS = 12
DEFAULT_COMPONENTS = [
    0.0, 0.0, 0.0, 0.0,  # Position, padded
    0.0, 0.0, 0.0, 1.0,  # Quaternion
    1.0, 1.0, 1.0, 0.0  # Scale, padded
]


def quaternion_to_matrix(quaternion):
    """Convert quaternion to rotation matrix."""
    w, x, y, z = quaternion
    return np.array([
        [1 - 2*y**2 - 2*z**2,     2*x*y - 2*z*w,       2*x*z + 2*y*w],
        [2*x*y + 2*z*w,           1 - 2*x**2 - 2*z**2, 2*y*z - 2*x*w],
        [2*x*z - 2*y*w,           2*y*z + 2*x*w,       1 - 2*x**2 - 2*y**2]
    ])


def compose_matrix(position, quaternion, scale):
    """Compose a transformation matrix from position, quaternion, and scale."""
    T = np.eye(4)
    R = quaternion_to_matrix(quaternion)
    S = np.diag(np.append(scale, 1))
    T[:3, :3] = R
    T[:3, :3] = T[:3, :3] @ np.diag(scale)
    T[:3, 3] = position
    return T


def multiply_quaternions(q1, q2):
    w1, x1, y1, z1 = q1
    w2, x2, y2, z2 = q2
    w = w1 * w2 - x1 * x2 - y1 * y2 - z1 * z2
    x = w1 * x2 + x1 * w2 + y1 * z2 - z1 * y2
    y = w1 * y2 - x1 * z2 + y1 * w2 + z1 * x2
    z = w1 * z2 + x1 * y2 - y1 * x2 + z1 * w2
    return w, x, y, z


def decompose_matrix(matrix):
    """Decompose a transformation matrix into position, quaternion, and scale."""
    position = matrix[:3, 3]
    scale = np.linalg.norm(matrix[:3, :3], axis=0)
    norm_matrix = matrix[:3, :3] / scale
    quaternion = matrix_to_quaternion(norm_matrix)
    return position, quaternion, scale


def matrix_to_quaternion(matrix):
    """Convert a rotation matrix to a quaternion."""
    m = matrix
    trace = np.trace(m)

    if trace > 0:
        s = np.sqrt(trace + 1.0) * 2
        qw = 0.25 * s
        qx = (m[2, 1] - m[1, 2]) / s
        qy = (m[0, 2] - m[2, 0]) / s
        qz = (m[1, 0] - m[0, 1]) / s
    elif (m[0, 0] > m[1, 1]) and (m[0, 0] > m[2, 2]):
        s = np.sqrt(1.0 + m[0, 0] - m[1, 1] - m[2, 2]) * 2
        qw = (m[2, 1] - m[1, 2]) / s
        qx = 0.25 * s
        qy = (m[0, 1] + m[1, 0]) / s
        qz = (m[0, 2] + m[2, 0]) / s
    elif m[1, 1] > m[2, 2]:
        s = np.sqrt(1.0 + m[1, 1] - m[0, 0] - m[2, 2]) * 2
        qw = (m[0, 2] - m[2, 0]) / s
        qx = (m[0, 1] + m[1, 0]) / s
        qy = 0.25 * s
        qz = (m[1, 2] + m[2, 1]) / s
    else:
        s = np.sqrt(1.0 + m[2, 2] - m[0, 0] - m[1, 1]) * 2
        qw = (m[1, 0] - m[0, 1]) / s
        qx = (m[0, 2] + m[2, 0]) / s
        qy = (m[1, 2] + m[2, 1]) / s
        qz = 0.25 * s

    return np.array([qw, qx, qy, qz])

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
            rotation_keys.append(
                (rotation_key.value[0], rotation_key.value[1], rotation_key.value[2], rotation_key.value[3]))

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

    # for frame in frames:
    #     for bone_id, bone in enumerate(frame["bones"]):
    #         bone_name = id_to_name[bone_id]
    #         parent_id = skeleton_data[bone_name]["parentId"]
    #         acc_pos = np.array(bone[0:3])
    #         acc_rot = np.array(bone[4:8])
    #         acc_scale = np.array(bone[8:11])
    #         while parent_id is not None:
    #             parent_bone = frame["bones"][parent_id]

    #             # Position
    #             parent_pos = np.array(parent_bone[0:3])
    #             parent_rot = np.array(parent_bone[4:8])
    #             parent_scale = np.array(parent_bone[8:11])

    #             parent_matrix = compose_matrix(parent_pos, parent_rot, parent_scale)
    #             bone_matrix = compose_matrix(acc_pos, acc_rot, acc_scale)
    #             bone_matrix = parent_matrix @ bone_matrix
    #             acc_pos, acc_rot, acc_scale = decompose_matrix(bone_matrix)

    #             new_parent_id = skeleton_data[id_to_name[parent_id]]["parentId"]
    #             if new_parent_id is None:
    #                 break
    #             else:
    #                 parent_id = new_parent_id

    #         bone[0:3] = acc_pos
    #         bone[4:8] = acc_rot
    #         bone[8:11] = acc_scale

    #         frames[i]["bones"][bone_id] = bone

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

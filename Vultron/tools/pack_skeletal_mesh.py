import argparse
import struct
# from impasse import load
# from impasse.constants import ProcessingPreset, ProcessingStep
from pyassimp import load, postprocess
import numpy as np

# Take a gltf or glb file and pack it into a vultron mesh file
def main():
    parser = argparse.ArgumentParser(description="Pack a gltf or glb file into a vultron mesh file")
    # pack_mesh.py inputmesh.gltf -o outputmesh.bin
    parser.add_argument("input", help="The input gltf or glb file")
    parser.add_argument("-o", "--output", help="The output vultron mesh file", default="mesh.bin")
    args = parser.parse_args()

    with load(args.input, "fbx", processing=(0x4000 | postprocess.aiProcess_Triangulate)) as scene:
        # Put all bones from all mesghes in a set
        bones = set()
        for mesh in scene.meshes:
            # bones.update([bone.name for bone in mesh.bones])
            for bone in mesh.bones:
                try:
                    print(bone.weights[0].__dict__)
                    exit()
                except:
                    pass
        

    # for mesh in scene.meshes:
    #     print(mesh)

    # print(f"Found {len(bones)} bones")





if __name__ == "__main__":
    main()
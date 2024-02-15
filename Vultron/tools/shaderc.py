#!/usr/local/bin/python3.11
import argparse
import os
import subprocess
import termcolor

VULKAN_SDK_PATH = os.environ.get("VULKAN_SDK")
GLSLC_PATH = f"{VULKAN_SDK_PATH}/bin/" if VULKAN_SDK_PATH else ""


def gen_output(source, output):
    # Create a temporary file to store the source
    with open("temp.glsl", "w") as file:
        file.write(source)

    try:
        # Compile the source to SPIR-V
        subprocess.run([f"{GLSLC_PATH}glslc", "temp.glsl", "-o", f"{output}"])
    except FileNotFoundError:
        print(f"Error: {GLSLC_PATH} not found")
    except Exception as e:
        print(f"Error: {e}")

    os.remove("temp.glsl")


def main():
    parser = argparse.ArgumentParser(description="Compile GLSL to SPIR-V")
    parser.add_argument("source", help="Path to the GLSL source file")
    parser.add_argument("output", help="Path to the output SPIR-V file")

    args = parser.parse_args()
    gen_output(args.source, args.output)


if __name__ == "__main__":
    main()

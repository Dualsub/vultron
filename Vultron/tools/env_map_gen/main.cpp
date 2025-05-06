#include "Vultron/SceneRenderer.h"

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <future>
#include <fstream>

using namespace Vultron;

int main(int argc, char **argv)
{
    std::vector<std::string> args(argv, argv + argc);

    if (std::find(args.begin(), args.end(), "-h") != args.end() ||
        std::find(args.begin(), args.end(), "--help") != args.end() ||
        std::find(args.begin(), args.end(), "help") != args.end())
    {
        std::cout << "Usage: env_map_gen [options] <type> <input_file> <output_file>" << std::endl;
        std::cout << "Options:" << std::endl;
        std::cout << "  -h, --help, help  Display this help message" << std::endl;
        return 0;
    }

    if (args.size() != 4)
    {
        std::cerr << "Invalid number of arguments. Use -h for help." << std::endl;
        return 1;
    }

    std::string type = args[1];
    std::string inputFile = args[2];
    std::string outputFile = args[3];

    if (!std::filesystem::exists(inputFile))
    {
        std::cerr << "Input file does not exist." << std::endl;
        return 1;
    }

    Window window;
    window.Initialize({.title = "Environment Map Generator"});

    VulkanRenderer renderer;
    renderer.Initialize(window);

    RenderHandle skybox = type != "sh2cm" ? renderer.LoadImage(inputFile, ImageType::CubemapArray, true) : RenderHandle();

    auto future = std::async(std::launch::async, [&renderer]
                             { renderer.WaitAndResetImageTransitionQueue(); });

    while (future.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
    {
        RenderData renderData = {
            .staticBatches = {},
            .staticInstances = {},
            .skeletalBatches = {},
            .skeletalInstances = {},
            .animationInstances = {},
            .decalInstances = {},
            .spriteBatches = {},
            .sdfBatches = {},
            .spriteInstances = {},
            .particleEmitters = {},
            .environmentMap = {},
            .particleAtlasMaterial = {},
            .pointLights = {},
            .lines = {},
            .ribbonVertices = {},
            .ribbonIndices = {},
        };
        renderer.Draw(renderData);
    }

    RenderHandle outputImage;
    if (type == "irradiance")
    {
        outputImage = renderer.GenerateIrradianceMap(skybox, outputFile);
        renderer.SaveImage(outputImage, outputFile, false);
    }
    else if (type == "sh")
    {
        std::cout << "Generating SH data..." << std::endl;
        std::vector<SHData> shData = renderer.GenerateIrradianceSHs(skybox);
        std::ofstream shOut(outputFile, std::ios::binary | std::ios::out);
        IrradianceVolumeData ivData;
        ivData.volume.numCells = glm::uvec4(1);
        ivData.volume.min = glm::vec4(0.0f);
        ivData.volume.max = glm::vec4(0.0f);
        shOut.write(reinterpret_cast<const char *>(&ivData.volume), sizeof(IrradianceVolumeData::volume));
        shOut.write(reinterpret_cast<const char *>(shData.data()), shData.size() * sizeof(SHData));
        shOut.close();
    }
    else if (type == "prefilter")
    {
        outputImage = renderer.GeneratePrefilteredMap(skybox, outputFile);
        renderer.SaveImage(outputImage, outputFile, false);
    }
    else if (type == "sh2cm")
    {
        std::cout << "Loading SH data..." << std::endl;
        std::vector<SHData> shData;
        std::fstream file(inputFile, std::ios::in | std::ios::binary);
        IrradianceVolumeData ivData;
        file.read(reinterpret_cast<char *>(&ivData.volume), sizeof(IrradianceVolumeData::volume));
        const size_t numSHs = ivData.volume.numCells.x * ivData.volume.numCells.y * ivData.volume.numCells.z;
        outputImage = renderer.GenerateCubemapFromSHs(shData, outputFile);
        renderer.SaveImage(outputImage, outputFile, false);
    }
    else
    {
        std::cerr << "Invalid type. Use -h for help." << std::endl;
        return 1;
    }

    renderer.Shutdown();
    window.Shutdown();

    return 0;
}
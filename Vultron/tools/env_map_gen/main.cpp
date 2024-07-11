#include "Vultron/SceneRenderer.h"

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>

using namespace Vultron;

int main(int argc, char** argv) 
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

    if (args.size() != 3) 
    {
        std::cerr << "Invalid number of arguments. Use -h for help." << std::endl;
        return 1;
    }

    std::string type = args[1];
    std::string input_file = args[2];
    std::string output_file = args[3];

    if (!std::filesystem::exists(input_file)) 
    {
        std::cerr << "Input file does not exist." << std::endl;
        return 1;
    }

    Window window;
    window.Initialize({.title="Environment Map Generator"});

    VulkanRenderer renderer;
    renderer.Initialize(window);

    RenderHandle skybox = renderer.LoadImage(input_file);
    
    RenderHandle outputImage;
    if (type == "irradiance") 
    {
        outputImage = renderer.GenerateIrradianceMap(skybox, output_file);
    } 
    else if (type == "prefilter") 
    {
        outputImage = renderer.GeneratePrefilteredMap(skybox, output_file);
    } 
    else 
    {
        std::cerr << "Invalid type. Use -h for help." << std::endl;
        return 1;
    } 
    
    renderer.SaveImage(outputImage, output_file);

    renderer.Shutdown();
    window.Shutdown();

    return 0;
}
#include "Vultron/Vulkan/Utils.h"

#include <vector>

namespace Vultron
{
    QueueFamilies QueryQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
    {
        QueueFamilies families;
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto &queueFamily : queueFamilies)
        {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                families.graphicsFamily = i;
            }

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

            if (presentSupport)
            {
                families.presentFamily = i;
            }

            if (families.IsComplete())
            {
                break;
            }

            i++;
        }

        return families;
    }
}
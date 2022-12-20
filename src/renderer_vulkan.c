#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "renderer_vulkan.h"
#include "window.h"

#define INVALID_RENDERER 0xFFFFFFFF

typedef struct renderer_data_s {
    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice physicalDevice;
    u32 graphicsQueueFamilyIndex;
    u32 presentQueueFamilyIndex;
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
} RendererData;

Renderer renderer_vulkan_renderer_limit = 0;
Renderer renderer_vulkan_renderer_count = 0;
RendererData *renderer_vulkan_renderers_data = NULL;

void renderer_vulkan_ensure_space_available() {
    if (renderer_vulkan_renderer_count == renderer_vulkan_renderer_limit) {
        renderer_vulkan_renderer_limit = renderer_vulkan_renderer_limit * 2 + 1;
        RendererData *old_data = renderer_vulkan_renderers_data;
        renderer_vulkan_renderers_data = malloc(sizeof(RendererData) * renderer_vulkan_renderer_limit);
        if (old_data != NULL) {
            memcpy(renderer_vulkan_renderers_data, old_data, sizeof(RendererData) * renderer_vulkan_renderer_count);
            free(old_data);
        }
    }
}

VkResult renderer_vulkan_create_instance(RendererData *rendererData, Window window) {
    VkApplicationInfo applicationInfo;
    applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pNext = NULL;
    applicationInfo.pApplicationName = "CGFS";
    applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    applicationInfo.pEngineName = "CGFS";
    applicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    applicationInfo.apiVersion = VK_API_VERSION_1_0;

    u32 extension_count = window_enumerate_required_vulkan_extensions(window, NULL);
    const char *extensions[extension_count];
    window_enumerate_required_vulkan_extensions(window, extensions);

    VkInstanceCreateInfo instanceCreateInfo;
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pNext = NULL;
    instanceCreateInfo.flags = 0;
    instanceCreateInfo.pApplicationInfo = &applicationInfo;
    instanceCreateInfo.enabledLayerCount = 0;
    instanceCreateInfo.ppEnabledLayerNames = NULL;
    instanceCreateInfo.enabledExtensionCount = extension_count;
    instanceCreateInfo.ppEnabledExtensionNames = extensions;

    return vkCreateInstance(&instanceCreateInfo, NULL, &rendererData->instance);
}

bool renderer_vulkan_is_physical_device_usable(RendererData *rendererData,
                                               VkPhysicalDevice physicalDevice,
                                               VkPhysicalDeviceType *pPhysicalDeviceType,
                                               u32 *pGraphicsQueueFamilyIndex,
                                               u32 *pPresentQueueFamilyIndex) {
    u32 graphicsQueueFamilyIndex = -1;
    u32 presentQueueFamilyIndex = -1;
    u32 queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, NULL);
    if (queueFamilyCount == 0) {
        return false;
    }
    VkQueueFamilyProperties *pQueueFamilyProperties = malloc(sizeof(VkQueueFamilyProperties) * queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, pQueueFamilyProperties);
    if (queueFamilyCount == 0) {
        free(pQueueFamilyProperties);
        return false;
    }
    for (int queueFamilyIndex = 0; queueFamilyIndex < queueFamilyCount; queueFamilyIndex++) {
        if (graphicsQueueFamilyIndex == -1 &&
            (pQueueFamilyProperties[queueFamilyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            graphicsQueueFamilyIndex = queueFamilyIndex;
        }
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFamilyIndex, rendererData->surface,
                                             &presentSupport);
        if (presentSupport) {
            presentQueueFamilyIndex = queueFamilyIndex;
        }
    }
    free(pQueueFamilyProperties);
    if (graphicsQueueFamilyIndex == -1 || presentQueueFamilyIndex == -1) {
        return false;
    }
    u32 availableExtensionCount = 0;
    vkEnumerateDeviceExtensionProperties(physicalDevice, NULL, &availableExtensionCount, NULL);
    VkExtensionProperties *availableExtensions = malloc(sizeof(VkExtensionProperties) * availableExtensionCount);
    vkEnumerateDeviceExtensionProperties(physicalDevice, NULL, &availableExtensionCount, availableExtensions);
//        u32 requiredExtensionCount = window_enumerate_required_vulkan_extensions(window, NULL) + 1;
    u32 requiredExtensionCount = 1;
    const char *requiredExtensions[requiredExtensionCount];
//        window_enumerate_required_vulkan_extensions(window, requiredExtensions);
    requiredExtensions[requiredExtensionCount - 1] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    u32 requiredExtensionFoundCount = 0;
    for (int i = 0; i < requiredExtensionCount; i++) {
        for (int j = 0; j < availableExtensionCount; j++) {
            if (strcmp(requiredExtensions[i], availableExtensions[j].extensionName) == 0) {
                requiredExtensionFoundCount++;
            }
        }
    }
    free(availableExtensions);
    if (requiredExtensionFoundCount != requiredExtensionCount) {
        return false;
    }
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);
    vkGetPhysicalDeviceFeatures(physicalDevice, &features);
    *pPhysicalDeviceType = properties.deviceType;
    *pGraphicsQueueFamilyIndex = graphicsQueueFamilyIndex;
    *pPresentQueueFamilyIndex = presentQueueFamilyIndex;
    return true;
}

VkResult renderer_vulkan_find_usable_physical_device(RendererData *rendererData, Window window) {
    u32 deviceCount = 0;
    VkResult result = vkEnumeratePhysicalDevices(rendererData->instance, &deviceCount, NULL);
    if (result != VK_SUCCESS) {
        return result;
    }
    if (deviceCount == 0) {
        return VK_ERROR_UNKNOWN;
    }
    VkPhysicalDevice *physicalDevices = malloc(sizeof(VkPhysicalDevice) * deviceCount);
    result = vkEnumeratePhysicalDevices(rendererData->instance, &deviceCount, physicalDevices);
    if (result != VK_SUCCESS) {
        free(physicalDevices);
        return result;
    }
    if (deviceCount == 0) {
        free(physicalDevices);
        return VK_ERROR_UNKNOWN;
    }
    VkPhysicalDevice usablePhysicalDevice = NULL;
    u32 graphicsQueueFamilyIndex;
    u32 presentQueueFamilyIndex;
    for (int physicalDeviceIndex = 0; physicalDeviceIndex < deviceCount; physicalDeviceIndex++) {
        VkPhysicalDevice physicalDevice = physicalDevices[physicalDeviceIndex];
        VkPhysicalDeviceType physicalDeviceType;
        if (renderer_vulkan_is_physical_device_usable(rendererData, physicalDevice, &physicalDeviceType,
                                                      &graphicsQueueFamilyIndex, &presentQueueFamilyIndex)) {
            usablePhysicalDevice = physicalDevice;
            if (physicalDeviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                break;
            }
        }
    }
    free(physicalDevices);
    if (usablePhysicalDevice) {
        rendererData->physicalDevice = usablePhysicalDevice;
        rendererData->graphicsQueueFamilyIndex = graphicsQueueFamilyIndex;
        rendererData->presentQueueFamilyIndex = presentQueueFamilyIndex;
        return VK_SUCCESS;
    }
    return VK_ERROR_UNKNOWN;
}

void renderer_vulkan_init_device_queue_create_info(u32 queueFamilyIndex, u32 queueCount,
                                                   VkDeviceQueueCreateInfo *deviceQueueCreateInfo) {
    float queuePriority = 1.0f;
    deviceQueueCreateInfo->sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    deviceQueueCreateInfo->pNext = NULL;
    deviceQueueCreateInfo->flags = 0;
    deviceQueueCreateInfo->queueFamilyIndex = queueFamilyIndex;
    deviceQueueCreateInfo->queueCount = queueCount;
    deviceQueueCreateInfo->pQueuePriorities = &queuePriority;
}

u32 renderer_vulkan_get_unique_u32(u32 *result, u32 num, ...) {
    int i;
    va_list valist;
    va_start(valist, num);
    result[0] = va_arg(valist, u32);
    for (i = 1; i < num; i++) {
        u32 value = va_arg(valist, u32);
        bool unique = true;
        for (int j = 0; j < i; j++) {
            if (value == result[j]) {
                unique = false;
                break;
            }
        }
        if (unique) {
            result[i] = value;
        }
    }
    va_end(valist);
    return i;
}

VkResult renderer_vulkan_create_device(RendererData *rendererData) {
    u32 queueFamilyIndices[2];
    u32 queueCreateInfoCount = renderer_vulkan_get_unique_u32(queueFamilyIndices,
                                                              rendererData->graphicsQueueFamilyIndex,
                                                              rendererData->presentQueueFamilyIndex);
    VkDeviceQueueCreateInfo queueCreateInfos[queueCreateInfoCount];
    for (int i = 0; i < queueCreateInfoCount; i++) {
        renderer_vulkan_init_device_queue_create_info(queueFamilyIndices[i], 1, &queueCreateInfos[i]);
    }

    VkPhysicalDeviceFeatures physicalDeviceFeatures;
    memset(&physicalDeviceFeatures, 0, sizeof(VkPhysicalDeviceFeatures));

    const char *const enabledExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    VkDeviceCreateInfo deviceCreateInfo;
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = NULL;
    deviceCreateInfo.flags = 0;
    deviceCreateInfo.queueCreateInfoCount = queueCreateInfoCount;
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos;
    deviceCreateInfo.enabledLayerCount = 0;
    deviceCreateInfo.ppEnabledLayerNames = NULL;
    deviceCreateInfo.enabledExtensionCount = 1;
    deviceCreateInfo.ppEnabledExtensionNames = enabledExtensions;
    deviceCreateInfo.pEnabledFeatures = &physicalDeviceFeatures;

    return vkCreateDevice(rendererData->physicalDevice, &deviceCreateInfo, NULL, &rendererData->device);
}

Renderer renderer_create(Window window) {
    renderer_vulkan_ensure_space_available();
    RendererData *rendererData = &renderer_vulkan_renderers_data[renderer_vulkan_renderer_count];
    if (renderer_vulkan_create_instance(rendererData, window) != VK_SUCCESS) {
        return INVALID_RENDERER;
    }
    if (window_create_vulkan_surface(window, rendererData->instance, &rendererData->surface) != VK_SUCCESS) {
        return INVALID_RENDERER;
    }
    if (renderer_vulkan_find_usable_physical_device(rendererData, window) != VK_SUCCESS) {
        return INVALID_RENDERER;
    }
    if (renderer_vulkan_create_device(rendererData) != VK_SUCCESS) {
        return INVALID_RENDERER;
    }
    vkGetDeviceQueue(rendererData->device, rendererData->graphicsQueueFamilyIndex, 0, &rendererData->graphicsQueue);
    vkGetDeviceQueue(rendererData->device, rendererData->presentQueueFamilyIndex, 0, &rendererData->presentQueue);
    return renderer_vulkan_renderer_count++;
}

void renderer_destroy(Renderer renderer) {
    if (renderer == INVALID_RENDERER) {
        return;
    }
    vkDestroyDevice(renderer_vulkan_renderers_data[renderer].device, NULL);
    vkDestroyInstance(renderer_vulkan_renderers_data[renderer].instance, NULL);
}

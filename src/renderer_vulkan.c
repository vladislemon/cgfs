#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include "renderer_vulkan.h"
#include "window.h"

#define INVALID_RENDERER 0xFFFFFFFF
#define VULKAN_VALIDATION_LAYER_NAME "VK_LAYER_KHRONOS_validation"

typedef struct renderer_data_s {
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkSurfaceKHR surface;
    VkPhysicalDevice physicalDevice;
    u32 graphicsQueueFamilyIndex;
    u32 presentQueueFamilyIndex;
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    VkSurfaceFormatKHR surfaceFormat;
    VkPresentModeKHR presentMode;
    VkExtent2D swapExtent;
    VkSwapchainKHR swapchain;
    u32 swapchainImageCount;
    VkImage *swapchainImages;
    VkImageView *swapchainImageViews;
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

VKAPI_ATTR VkBool32 VKAPI_CALL renderer_vulkan_debug_callback(
        __attribute__((unused)) VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        __attribute__((unused)) VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
        __attribute__((unused)) void *pUserData
) {
    printf("VK_VALIDATION: %s\n", pCallbackData->pMessage);
    return VK_FALSE;
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

    u32 extension_count = window_enumerate_required_vulkan_extensions(window, NULL) + 1;
    const char *extensions[extension_count];
    window_enumerate_required_vulkan_extensions(window, extensions);
    extensions[extension_count - 1] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;

    u32 layer_count = 1;
    const char *layers[layer_count];
    layers[0] = VULKAN_VALIDATION_LAYER_NAME;

    VkInstanceCreateInfo instanceCreateInfo;
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pNext = NULL;
    instanceCreateInfo.flags = 0;
    instanceCreateInfo.pApplicationInfo = &applicationInfo;
    instanceCreateInfo.enabledLayerCount = layer_count;
    instanceCreateInfo.ppEnabledLayerNames = layers;
    instanceCreateInfo.enabledExtensionCount = extension_count;
    instanceCreateInfo.ppEnabledExtensionNames = extensions;

    return vkCreateInstance(&instanceCreateInfo, NULL, &rendererData->instance);
}

VkResult renderer_vulkan_create_debug_messenger(RendererData *rendererData) {
    VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo;
    debugMessengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugMessengerCreateInfo.pNext = NULL;
    debugMessengerCreateInfo.flags = 0;
    debugMessengerCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debugMessengerCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debugMessengerCreateInfo.pfnUserCallback = renderer_vulkan_debug_callback;
    debugMessengerCreateInfo.pUserData = NULL;
    PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(
            rendererData->instance, "vkCreateDebugUtilsMessengerEXT");
    VkResult result = func(rendererData->instance, &debugMessengerCreateInfo, NULL, &rendererData->debugMessenger);
    return result;
}

bool renderer_vulkan_choose_swap_surface_format(RendererData *rendererData, VkPhysicalDevice physicalDevice,
                                                VkSurfaceFormatKHR *pSurfaceFormat) {
    u32 surfaceFormatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, rendererData->surface, &surfaceFormatCount, NULL);
    if (surfaceFormatCount < 1) {
        return false;
    }
    VkSurfaceFormatKHR *surfaceFormats = malloc(sizeof(VkSurfaceFormatKHR) * surfaceFormatCount);
    if (surfaceFormats == NULL) {
        return false;
    }
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, rendererData->surface, &surfaceFormatCount, surfaceFormats);
    for (int i = 0; i < surfaceFormatCount; i++) {
        VkSurfaceFormatKHR availableFormat = surfaceFormats[i];
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            *pSurfaceFormat = availableFormat;
            break;
        }
    }
    free(surfaceFormats);
    return true;
}

bool renderer_vulkan_choose_swap_present_mode(RendererData *rendererData, VkPhysicalDevice physicalDevice,
                                              VkPresentModeKHR *pPresentMode) {
    *pPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    u32 presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, rendererData->surface, &presentModeCount, NULL);
    if (presentModeCount < 1) {
        return false;
    }
    VkPresentModeKHR *presentModes = malloc(sizeof(VkPresentModeKHR) * presentModeCount);
    if (presentModes == NULL) {
        return false;
    }
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, rendererData->surface, &presentModeCount, presentModes);
    for (int i = 0; i < presentModeCount; i++) {
        VkPresentModeKHR availableMode = presentModes[i];
        if (availableMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            *pPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        }
    }
    free(presentModes);
    return true;
}

bool renderer_vulkan_is_physical_device_usable(RendererData *rendererData,
                                               VkPhysicalDevice physicalDevice,
                                               VkPhysicalDeviceType *pPhysicalDeviceType,
                                               u32 *pGraphicsQueueFamilyIndex,
                                               u32 *pPresentQueueFamilyIndex,
                                               VkSurfaceFormatKHR *pSurfaceFormat,
                                               VkPresentModeKHR *pPresentMode) {
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
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFamilyIndex, rendererData->surface, &presentSupport);
        if (presentQueueFamilyIndex == -1 && presentSupport) {
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
    bool validSurfaceFormatAvailable = renderer_vulkan_choose_swap_surface_format(rendererData, physicalDevice,
                                                                                  pSurfaceFormat);
    if (!validSurfaceFormatAvailable) {
        return false;
    }
    bool validPresentModeAvailable = renderer_vulkan_choose_swap_present_mode(rendererData, physicalDevice,
                                                                              pPresentMode);
    if (!validPresentModeAvailable) {
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

VkResult renderer_vulkan_find_usable_physical_device(RendererData *rendererData) {
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
    VkSurfaceFormatKHR surfaceFormat;
    VkPresentModeKHR presentMode;
    for (int physicalDeviceIndex = 0; physicalDeviceIndex < deviceCount; physicalDeviceIndex++) {
        VkPhysicalDevice physicalDevice = physicalDevices[physicalDeviceIndex];
        VkPhysicalDeviceType physicalDeviceType;
        if (renderer_vulkan_is_physical_device_usable(rendererData, physicalDevice, &physicalDeviceType,
                                                      &graphicsQueueFamilyIndex, &presentQueueFamilyIndex,
                                                      &surfaceFormat, &presentMode)) {
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
        rendererData->surfaceFormat = surfaceFormat;
        rendererData->presentMode = presentMode;
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
    int i, uniqueCount = 1;
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
            uniqueCount++;
        }
    }
    va_end(valist);
    return uniqueCount;
}

VkResult renderer_vulkan_create_device(RendererData *rendererData) {
    u32 queueFamilyIndices[2];
    u32 queueCreateInfoCount = renderer_vulkan_get_unique_u32(queueFamilyIndices,
                                                              2,
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

void renderer_vulkan_choose_swap_extent(Window window, VkSurfaceCapabilitiesKHR *surfaceCapabilities,
                                        VkExtent2D *extent) {
    u32 width, height;
    window_get_size_in_pixels(window, &width, &height);
    if (width < surfaceCapabilities->minImageExtent.width) {
        width = surfaceCapabilities->minImageExtent.width;
    } else if (width > surfaceCapabilities->maxImageExtent.width) {
        width = surfaceCapabilities->maxImageExtent.width;
    }
    if (height < surfaceCapabilities->minImageExtent.height) {
        height = surfaceCapabilities->minImageExtent.height;
    } else if (height > surfaceCapabilities->maxImageExtent.height) {
        height = surfaceCapabilities->maxImageExtent.height;
    }
    extent->width = width;
    extent->height = height;
}

u32 renderer_vulkan_choose_swap_image_count(VkSurfaceCapabilitiesKHR *surfaceCapabilities) {
    u32 imageCount = 3;
    if (imageCount < surfaceCapabilities->minImageCount) {
        imageCount = surfaceCapabilities->minImageCount;
    } else if (surfaceCapabilities->maxImageCount > 0 && imageCount > surfaceCapabilities->maxImageCount) {
        imageCount = surfaceCapabilities->maxImageCount;
    }
    return imageCount;
}

VkResult renderer_vulkan_create_swapchain(RendererData *rendererData, Window window) {
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(rendererData->physicalDevice, rendererData->surface,
                                              &surfaceCapabilities);
    renderer_vulkan_choose_swap_extent(window, &surfaceCapabilities, &rendererData->swapExtent);
    u32 imageCount = renderer_vulkan_choose_swap_image_count(&surfaceCapabilities);
    VkSwapchainCreateInfoKHR swapchainCreateInfo;
    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.pNext = NULL;
    swapchainCreateInfo.flags = 0;
    swapchainCreateInfo.surface = rendererData->surface;
    swapchainCreateInfo.minImageCount = imageCount;
    swapchainCreateInfo.imageFormat = rendererData->surfaceFormat.format;
    swapchainCreateInfo.imageColorSpace = rendererData->surfaceFormat.colorSpace;
    swapchainCreateInfo.imageExtent = rendererData->swapExtent;
    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    u32 queueFamilyIndices[] = {rendererData->graphicsQueueFamilyIndex, rendererData->presentQueueFamilyIndex};
    if (queueFamilyIndices[0] != queueFamilyIndices[1]) {
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchainCreateInfo.queueFamilyIndexCount = 2;
        swapchainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchainCreateInfo.queueFamilyIndexCount = 0;
        swapchainCreateInfo.pQueueFamilyIndices = NULL;
    }
    swapchainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
    swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainCreateInfo.presentMode = rendererData->presentMode;
    swapchainCreateInfo.clipped = VK_TRUE;
    swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;
    VkResult result = vkCreateSwapchainKHR(rendererData->device, &swapchainCreateInfo, NULL, &rendererData->swapchain);
    if (result != VK_SUCCESS) {
        return result;
    }
    vkGetSwapchainImagesKHR(rendererData->device, rendererData->swapchain, &rendererData->swapchainImageCount, NULL);
    rendererData->swapchainImages = malloc(sizeof(VkImage) * rendererData->swapchainImageCount);
    return vkGetSwapchainImagesKHR(rendererData->device, rendererData->swapchain, &rendererData->swapchainImageCount,
                                   rendererData->swapchainImages);
}

VkResult renderer_vulkan_create_swapchain_image_views(RendererData *rendererData) {
    VkResult result = VK_SUCCESS;
    rendererData->swapchainImageViews = malloc(sizeof(VkImageView) * rendererData->swapchainImageCount);
    for (int i = 0; i < rendererData->swapchainImageCount; ++i) {
        VkImageViewCreateInfo imageViewCreateInfo;
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.pNext = NULL;
        imageViewCreateInfo.flags = 0;
        imageViewCreateInfo.image = rendererData->swapchainImages[i];
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.format = rendererData->surfaceFormat.format;
        imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        imageViewCreateInfo.subresourceRange.levelCount = 1;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.layerCount = 1;
        result = vkCreateImageView(rendererData->device, &imageViewCreateInfo, NULL,
                                   &rendererData->swapchainImageViews[i]);
        if (result != VK_SUCCESS) {
            break;
        }
    }
    return result;
}

Renderer renderer_create(Window window) {
    renderer_vulkan_ensure_space_available();
    RendererData *rendererData = &renderer_vulkan_renderers_data[renderer_vulkan_renderer_count];
    if (renderer_vulkan_create_instance(rendererData, window) != VK_SUCCESS) {
        return INVALID_RENDERER;
    }
    if (renderer_vulkan_create_debug_messenger(rendererData) != VK_SUCCESS) {
        return INVALID_RENDERER;
    }
    if (window_create_vulkan_surface(window, rendererData->instance, &rendererData->surface) != VK_SUCCESS) {
        return INVALID_RENDERER;
    }
    if (renderer_vulkan_find_usable_physical_device(rendererData) != VK_SUCCESS) {
        return INVALID_RENDERER;
    }
    if (renderer_vulkan_create_device(rendererData) != VK_SUCCESS) {
        return INVALID_RENDERER;
    }
    if (renderer_vulkan_create_swapchain(rendererData, window) != VK_SUCCESS) {
        return INVALID_RENDERER;
    }
    if (renderer_vulkan_create_swapchain_image_views(rendererData) != VK_SUCCESS) {
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
    RendererData data = renderer_vulkan_renderers_data[renderer];
    for (int i = 0; i < data.swapchainImageCount; ++i) {
        vkDestroyImageView(data.device, data.swapchainImageViews[i], NULL);
    }
    free(data.swapchainImageViews);
    free(data.swapchainImages);
    vkDestroySwapchainKHR(data.device, data.swapchain, NULL);
    vkDestroyDevice(data.device, NULL);
    vkDestroySurfaceKHR(data.instance, data.surface, NULL);
    PFN_vkDestroyDebugUtilsMessengerEXT destroyDebugMessengerFunc = (PFN_vkDestroyDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(data.instance, "vkDestroyDebugUtilsMessengerEXT");
    destroyDebugMessengerFunc(data.instance, data.debugMessenger, NULL);
    vkDestroyInstance(data.instance, NULL);
}

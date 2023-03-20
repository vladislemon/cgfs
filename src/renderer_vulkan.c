#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include "renderer_vulkan.h"
#include "window.h"

#define INVALID_RENDERER 0xFFFFFFFF
#define VULKAN_VALIDATION_LAYER_NAME "VK_LAYER_KHRONOS_validation"
#define MAX_FRAMES_IN_FLIGHT 2

typedef struct renderer_data_s {
    Window window;
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
    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;
    VkFramebuffer *swapchainFramebuffers;
    VkCommandPool commandPool;
    VkCommandBuffer *commandBuffers;
    VkSemaphore *imageAvailableSemaphores;
    VkSemaphore *renderFinishedSemaphores;
    VkFence *inFlightFences;
    u32 currentFrame;
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

VkResult renderer_vulkan_create_instance(RendererData *rendererData) {
    VkApplicationInfo applicationInfo;
    applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pNext = NULL;
    applicationInfo.pApplicationName = "CGFS";
    applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    applicationInfo.pEngineName = "CGFS";
    applicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    applicationInfo.apiVersion = VK_API_VERSION_1_0;

    u32 extension_count = window_enumerate_required_vulkan_extensions(rendererData->window, NULL) + 1;
    const char *extensions[extension_count];
    window_enumerate_required_vulkan_extensions(rendererData->window, extensions);
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

VkResult renderer_vulkan_create_swapchain(RendererData *rendererData) {
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(rendererData->physicalDevice, rendererData->surface,
                                              &surfaceCapabilities);
    renderer_vulkan_choose_swap_extent(rendererData->window, &surfaceCapabilities, &rendererData->swapExtent);
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
    if (rendererData->swapchainImages != NULL) {
        free(rendererData->swapchainImages);
    }
    rendererData->swapchainImages = malloc(sizeof(VkImage) * rendererData->swapchainImageCount);
    return vkGetSwapchainImagesKHR(rendererData->device, rendererData->swapchain, &rendererData->swapchainImageCount,
                                   rendererData->swapchainImages);
}

VkResult renderer_vulkan_create_swapchain_image_views(RendererData *rendererData) {
    VkResult result = VK_SUCCESS;
    if (rendererData->swapchainImageViews != NULL) {
        free(rendererData->swapchainImageViews);
    }
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

VkResult renderer_vulkan_create_render_pass(RendererData *rendererData) {
    VkAttachmentDescription attachmentDescription;
    attachmentDescription.flags = 0;
    attachmentDescription.format = rendererData->surfaceFormat.format;
    attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference attachmentReference;
    attachmentReference.attachment = 0;
    attachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpassDescription;
    subpassDescription.flags = 0;
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.inputAttachmentCount = 0;
    subpassDescription.pInputAttachments = NULL;
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &attachmentReference;
    subpassDescription.pResolveAttachments = NULL;
    subpassDescription.pDepthStencilAttachment = NULL;
    subpassDescription.preserveAttachmentCount = 0;
    subpassDescription.pPreserveAttachments = NULL;

    VkSubpassDependency subpassDependency;
    subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependency.dstSubpass = 0;
    subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependency.srcAccessMask = 0;
    subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    subpassDependency.dependencyFlags = 0;

    VkRenderPassCreateInfo renderPassCreateInfo;
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.pNext = NULL;
    renderPassCreateInfo.flags = 0;
    renderPassCreateInfo.attachmentCount = 1;
    renderPassCreateInfo.pAttachments = &attachmentDescription;
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &subpassDescription;
    renderPassCreateInfo.dependencyCount = 1;
    renderPassCreateInfo.pDependencies = &subpassDependency;

    return vkCreateRenderPass(rendererData->device, &renderPassCreateInfo, NULL, &rendererData->renderPass);
}

VkShaderModule renderer_vulkan_create_shader_module(
        RendererData *rendererData,
        usize shader_length,
        const u32 *shader_spv
) {
    VkShaderModuleCreateInfo shaderModuleCreateInfo;
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfo.pNext = NULL;
    shaderModuleCreateInfo.flags = 0;
    shaderModuleCreateInfo.codeSize = shader_length;
    shaderModuleCreateInfo.pCode = shader_spv;
    VkShaderModule shaderModule;
    if (vkCreateShaderModule(rendererData->device, &shaderModuleCreateInfo, NULL, &shaderModule) != VK_SUCCESS) {
        return NULL;
    }
    return shaderModule;
}

void renderer_vulkan_init_shader_stage_create_info(VkPipelineShaderStageCreateInfo *shaderStageCreateInfo,
                                                   VkShaderStageFlagBits stage, VkShaderModule module) {
    shaderStageCreateInfo->sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageCreateInfo->pNext = NULL;
    shaderStageCreateInfo->flags = 0;
    shaderStageCreateInfo->stage = stage;
    shaderStageCreateInfo->module = module;
    shaderStageCreateInfo->pName = "main";
    shaderStageCreateInfo->pSpecializationInfo = NULL;
}

VkResult renderer_vulkan_create_graphics_pipeline(
        RendererData *rendererData,
        usize vertex_shader_length,
        const u32 *vertex_shader_spv,
        usize fragment_shader_length,
        const u32 *fragment_shader_spv
) {
    VkShaderModule vertShaderModule = renderer_vulkan_create_shader_module(rendererData, vertex_shader_length,
                                                                           vertex_shader_spv);
    VkShaderModule fragShaderModule = renderer_vulkan_create_shader_module(rendererData, fragment_shader_length,
                                                                           fragment_shader_spv);
    VkPipelineShaderStageCreateInfo vertShaderStageInfo;
    renderer_vulkan_init_shader_stage_create_info(&vertShaderStageInfo, VK_SHADER_STAGE_VERTEX_BIT, vertShaderModule);
    VkPipelineShaderStageCreateInfo fragShaderStageInfo;
    renderer_vulkan_init_shader_stage_create_info(&fragShaderStageInfo, VK_SHADER_STAGE_FRAGMENT_BIT, fragShaderModule);
    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo;
    dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateCreateInfo.pNext = NULL;
    dynamicStateCreateInfo.flags = 0;
    dynamicStateCreateInfo.dynamicStateCount = sizeof(dynamicStates) / sizeof(VkDynamicState);
    dynamicStateCreateInfo.pDynamicStates = dynamicStates;

    VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo;
    vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputStateCreateInfo.pNext = NULL;
    vertexInputStateCreateInfo.flags = 0;
    vertexInputStateCreateInfo.vertexBindingDescriptionCount = 0;
    vertexInputStateCreateInfo.pVertexBindingDescriptions = NULL;
    vertexInputStateCreateInfo.vertexAttributeDescriptionCount = 0;
    vertexInputStateCreateInfo.pVertexAttributeDescriptions = NULL;

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo;
    inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyStateCreateInfo.pNext = NULL;
    inputAssemblyStateCreateInfo.flags = 0;
    inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport;
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float) rendererData->swapExtent.width;
    viewport.height = (float) rendererData->swapExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor;
    VkOffset2D offset = {0, 0};
    scissor.offset = offset;
    scissor.extent = rendererData->swapExtent;

    VkPipelineViewportStateCreateInfo viewportStateCreateInfo;
    viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportStateCreateInfo.pNext = NULL;
    viewportStateCreateInfo.flags = 0;
    viewportStateCreateInfo.viewportCount = 1;
    viewportStateCreateInfo.pViewports = &viewport;
    viewportStateCreateInfo.scissorCount = 1;
    viewportStateCreateInfo.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo;
    rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationStateCreateInfo.pNext = NULL;
    rasterizationStateCreateInfo.flags = 0;
    rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
    rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
    rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
    rasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
    rasterizationStateCreateInfo.depthBiasClamp = 0.0f;
    rasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;
    rasterizationStateCreateInfo.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo;
    multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleStateCreateInfo.pNext = NULL;
    multisampleStateCreateInfo.flags = 0;
    multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
    multisampleStateCreateInfo.minSampleShading = 1.0f;
    multisampleStateCreateInfo.pSampleMask = NULL;
    multisampleStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
    multisampleStateCreateInfo.alphaToOneEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachmentState;
    colorBlendAttachmentState.blendEnable = VK_FALSE;
    colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachmentState.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo;
    colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendStateCreateInfo.pNext = NULL;
    colorBlendStateCreateInfo.flags = 0;
    colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
    colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
    colorBlendStateCreateInfo.attachmentCount = 1;
    colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;
    colorBlendStateCreateInfo.blendConstants[0] = 0.0f;
    colorBlendStateCreateInfo.blendConstants[1] = 0.0f;
    colorBlendStateCreateInfo.blendConstants[2] = 0.0f;
    colorBlendStateCreateInfo.blendConstants[3] = 0.0f;

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo;
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.pNext = NULL;
    pipelineLayoutCreateInfo.flags = 0;
    pipelineLayoutCreateInfo.setLayoutCount = 0;
    pipelineLayoutCreateInfo.pSetLayouts = NULL;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    pipelineLayoutCreateInfo.pPushConstantRanges = NULL;

    VkResult result = vkCreatePipelineLayout(rendererData->device, &pipelineLayoutCreateInfo, NULL,
                                             &rendererData->pipelineLayout);
    if (result != VK_SUCCESS) {
        goto exit;
    }

    VkGraphicsPipelineCreateInfo pipelineCreateInfo;
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.pNext = NULL;
    pipelineCreateInfo.flags = 0;
    pipelineCreateInfo.stageCount = 2;
    pipelineCreateInfo.pStages = shaderStages;
    pipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
    pipelineCreateInfo.pTessellationState = NULL;
    pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
    pipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
    pipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
    pipelineCreateInfo.pDepthStencilState = NULL;
    pipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
    pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
    pipelineCreateInfo.layout = rendererData->pipelineLayout;
    pipelineCreateInfo.renderPass = rendererData->renderPass;
    pipelineCreateInfo.subpass = 0;
    pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineCreateInfo.basePipelineIndex = -1;

    result = vkCreateGraphicsPipelines(rendererData->device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, NULL,
                                       &rendererData->graphicsPipeline);

    exit:
    vkDestroyShaderModule(rendererData->device, fragShaderModule, NULL);
    vkDestroyShaderModule(rendererData->device, vertShaderModule, NULL);
    return result;
}

VkResult renderer_vulkan_create_framebuffers(RendererData *rendererData) {
    if (rendererData->swapchainFramebuffers != NULL) {
        free(rendererData->swapchainFramebuffers);
    }
    rendererData->swapchainFramebuffers = malloc(sizeof(VkFramebuffer) * rendererData->swapchainImageCount);
    for (int i = 0; i < rendererData->swapchainImageCount; i++) {
        VkFramebufferCreateInfo framebufferCreateInfo;
        framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferCreateInfo.pNext = NULL;
        framebufferCreateInfo.flags = 0;
        framebufferCreateInfo.renderPass = rendererData->renderPass;
        framebufferCreateInfo.attachmentCount = 1;
        framebufferCreateInfo.pAttachments = &rendererData->swapchainImageViews[i];
        framebufferCreateInfo.width = rendererData->swapExtent.width;
        framebufferCreateInfo.height = rendererData->swapExtent.height;
        framebufferCreateInfo.layers = 1;
        VkResult result = vkCreateFramebuffer(rendererData->device, &framebufferCreateInfo, NULL,
                                              &rendererData->swapchainFramebuffers[i]);
        if (result != VK_SUCCESS) {
            return result;
        }
    }
    return VK_SUCCESS;
}

VkResult renderer_vulkan_create_command_pool(RendererData *rendererData) {
    VkCommandPoolCreateInfo commandPoolCreateInfo;
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.pNext = NULL;
    commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolCreateInfo.queueFamilyIndex = rendererData->graphicsQueueFamilyIndex;
    return vkCreateCommandPool(rendererData->device, &commandPoolCreateInfo, NULL, &rendererData->commandPool);
}

VkResult renderer_vulkan_create_command_buffers(RendererData *rendererData) {
    if (rendererData->commandBuffers != NULL) {
        free(rendererData->commandBuffers);
    }
    rendererData->commandBuffers = malloc(sizeof(VkCommandBuffer) * MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo commandBufferAllocateInfo;
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.pNext = NULL;
    commandBufferAllocateInfo.commandPool = rendererData->commandPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
    return vkAllocateCommandBuffers(rendererData->device, &commandBufferAllocateInfo, rendererData->commandBuffers);
}

VkResult renderer_vulkan_create_sync_objects(RendererData *rendererData) {
    if (rendererData->imageAvailableSemaphores != NULL) {
        free(rendererData->imageAvailableSemaphores);
    }
    rendererData->imageAvailableSemaphores = malloc(sizeof(VkSemaphore) * MAX_FRAMES_IN_FLIGHT);
    if (rendererData->renderFinishedSemaphores != NULL) {
        free(rendererData->renderFinishedSemaphores);
    }
    rendererData->renderFinishedSemaphores = malloc(sizeof(VkSemaphore) * MAX_FRAMES_IN_FLIGHT);
    if (rendererData->inFlightFences != NULL) {
        free(rendererData->inFlightFences);
    }
    rendererData->inFlightFences = malloc(sizeof(VkFence) * MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreCreateInfo;
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCreateInfo.pNext = NULL;
    semaphoreCreateInfo.flags = 0;

    VkFenceCreateInfo fenceCreateInfo;
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.pNext = NULL;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkResult result;
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        result = vkCreateSemaphore(rendererData->device, &semaphoreCreateInfo, NULL,
                                   &rendererData->imageAvailableSemaphores[i]);
        if (result != VK_SUCCESS) {
            return result;
        }
        result = vkCreateSemaphore(rendererData->device, &semaphoreCreateInfo, NULL,
                                   &rendererData->renderFinishedSemaphores[i]);
        if (result != VK_SUCCESS) {
            return result;
        }
        result = vkCreateFence(rendererData->device, &fenceCreateInfo, NULL, &rendererData->inFlightFences[i]);
        if (result != VK_SUCCESS) {
            return result;
        }
    }
    return VK_SUCCESS;
}

Renderer renderer_create(
        Window window,
        usize vertex_shader_length,
        const u32 *vertex_shader_spv,
        usize fragment_shader_length,
        const u32 *fragment_shader_spv
) {
    renderer_vulkan_ensure_space_available();
    RendererData *rendererData = &renderer_vulkan_renderers_data[renderer_vulkan_renderer_count];
    memset(rendererData, 0, sizeof(RendererData));
    rendererData->window = window;
    if (renderer_vulkan_create_instance(rendererData) != VK_SUCCESS) {
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
    if (renderer_vulkan_create_swapchain(rendererData) != VK_SUCCESS) {
        return INVALID_RENDERER;
    }
    if (renderer_vulkan_create_swapchain_image_views(rendererData) != VK_SUCCESS) {
        return INVALID_RENDERER;
    }
    if (renderer_vulkan_create_render_pass(rendererData) != VK_SUCCESS) {
        return INVALID_RENDERER;
    }
    if (renderer_vulkan_create_graphics_pipeline(rendererData, vertex_shader_length, vertex_shader_spv,
                                                 fragment_shader_length, fragment_shader_spv) != VK_SUCCESS) {
        return INVALID_RENDERER;
    }
    if (renderer_vulkan_create_framebuffers(rendererData) != VK_SUCCESS) {
        return INVALID_RENDERER;
    }
    if (renderer_vulkan_create_command_pool(rendererData) != VK_SUCCESS) {
        return INVALID_RENDERER;
    }
    if (renderer_vulkan_create_command_buffers(rendererData) != VK_SUCCESS) {
        return INVALID_RENDERER;
    }
    if (renderer_vulkan_create_sync_objects(rendererData) != VK_SUCCESS) {
        return INVALID_RENDERER;
    }
    rendererData->currentFrame = 0;
    vkGetDeviceQueue(rendererData->device, rendererData->graphicsQueueFamilyIndex, 0, &rendererData->graphicsQueue);
    vkGetDeviceQueue(rendererData->device, rendererData->presentQueueFamilyIndex, 0, &rendererData->presentQueue);
    return renderer_vulkan_renderer_count++;
}

void renderer_reload(Renderer renderer) {
    if (renderer == INVALID_RENDERER) {
        return;
    }
    RendererData *rendererData = &renderer_vulkan_renderers_data[renderer];
    vkDeviceWaitIdle(rendererData->device);
    for (int i = 0; i < rendererData->swapchainImageCount; i++) {
        vkDestroyFramebuffer(rendererData->device, rendererData->swapchainFramebuffers[i], NULL);
        vkDestroyImageView(rendererData->device, rendererData->swapchainImageViews[i], NULL);
    }
    vkDestroySwapchainKHR(rendererData->device, rendererData->swapchain, NULL);
    renderer_vulkan_create_swapchain(rendererData);
    renderer_vulkan_create_swapchain_image_views(rendererData);
    renderer_vulkan_create_framebuffers(rendererData);
}

VkResult renderer_vulkan_record_command_buffer(RendererData *rendererData, uint32_t imageIndex) {
    VkCommandBuffer commandBuffer = rendererData->commandBuffers[rendererData->currentFrame];

    VkCommandBufferBeginInfo commandBufferBeginInfo;
    commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    commandBufferBeginInfo.pNext = NULL;
    commandBufferBeginInfo.flags = 0;
    commandBufferBeginInfo.pInheritanceInfo = NULL;
    VkResult result = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
    if (result != VK_SUCCESS) {
        return result;
    }
    VkRenderPassBeginInfo renderPassBeginInfo;
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.pNext = NULL;
    renderPassBeginInfo.renderPass = rendererData->renderPass;
    renderPassBeginInfo.framebuffer = rendererData->swapchainFramebuffers[imageIndex];
    VkOffset2D renderAreaOffset = {0, 0};
    renderPassBeginInfo.renderArea.offset = renderAreaOffset;
    renderPassBeginInfo.renderArea.extent = rendererData->swapExtent;
    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    renderPassBeginInfo.clearValueCount = 1;
    renderPassBeginInfo.pClearValues = &clearColor;
    vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, rendererData->graphicsPipeline);

    VkViewport viewport;
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float) rendererData->swapExtent.width;
    viewport.height = (float) rendererData->swapExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor;
    VkOffset2D scissorOffset = {0, 0};
    scissor.offset = scissorOffset;
    scissor.extent = rendererData->swapExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    vkCmdEndRenderPass(commandBuffer);
    return vkEndCommandBuffer(commandBuffer);
}

void renderer_draw_frame(Renderer renderer) {
    if (renderer == INVALID_RENDERER) {
        return;
    }
    RendererData *rendererData = &renderer_vulkan_renderers_data[renderer];
    VkCommandBuffer commandBuffer = rendererData->commandBuffers[rendererData->currentFrame];
    VkSemaphore imageAvailableSemaphore = rendererData->imageAvailableSemaphores[rendererData->currentFrame];
    VkSemaphore renderFinishedSemaphore = rendererData->renderFinishedSemaphores[rendererData->currentFrame];
    VkFence inFlightFence = rendererData->inFlightFences[rendererData->currentFrame];

    vkWaitForFences(rendererData->device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(rendererData->device, rendererData->swapchain, UINT64_MAX,
                                            imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        renderer_reload(renderer);
        return;
    }
    vkResetFences(rendererData->device, 1, &inFlightFence);
    vkResetCommandBuffer(commandBuffer, 0);
    renderer_vulkan_record_command_buffer(rendererData, imageIndex);

    VkSubmitInfo submitInfo;
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = NULL;
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &imageAvailableSemaphore;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &renderFinishedSemaphore;
    vkQueueSubmit(rendererData->graphicsQueue, 1, &submitInfo, inFlightFence);

    VkPresentInfoKHR presentInfo;
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = NULL;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinishedSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &rendererData->swapchain;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = NULL;
    result = vkQueuePresentKHR(rendererData->presentQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        renderer_reload(renderer);
    }

    rendererData->currentFrame = (rendererData->currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void renderer_destroy(Renderer renderer) {
    if (renderer == INVALID_RENDERER) {
        return;
    }
    RendererData data = renderer_vulkan_renderers_data[renderer];
    vkDeviceWaitIdle(data.device);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(data.device, data.imageAvailableSemaphores[i], NULL);
        vkDestroySemaphore(data.device, data.renderFinishedSemaphores[i], NULL);
        vkDestroyFence(data.device, data.inFlightFences[i], NULL);
    }
    free(data.imageAvailableSemaphores);
    free(data.renderFinishedSemaphores);
    free(data.inFlightFences);
    vkDestroyCommandPool(data.device, data.commandPool, NULL);
    for (int i = 0; i < data.swapchainImageCount; i++) {
        vkDestroyFramebuffer(data.device, data.swapchainFramebuffers[i], NULL);
    }
    free(data.swapchainFramebuffers);
    vkDestroyPipeline(data.device, data.graphicsPipeline, NULL);
    vkDestroyPipelineLayout(data.device, data.pipelineLayout, NULL);
    vkDestroyRenderPass(data.device, data.renderPass, NULL);
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

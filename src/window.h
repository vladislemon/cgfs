#ifndef CGFS_WINDOW_H
#define CGFS_WINDOW_H

#ifdef _WIN32
#include "window_win32.h"
#else
#include "window_xcb.h"
#endif

#include <vulkan/vulkan_core.h>
#include "types.h"

void window_global_poll_events();

void window_global_wait_events();

Window window_create(u16 width, u16 height, const char *title);

void window_destroy(Window window);

bool window_is_close_requested(Window window);

u32 window_enumerate_required_vulkan_extensions(Window window, const char **extensions);

VkResult window_create_vulkan_surface(Window window, VkInstance instance, VkSurfaceKHR *surface);

void window_get_size_in_pixels(Window window, u32 *width, u32 *height);

#endif //CGFS_WINDOW_H

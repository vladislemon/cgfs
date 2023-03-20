#ifndef _WIN32

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <xcb/xcb.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_xcb.h>
#include "window_xcb.h"

#define INVALID_WINDOW 0xFFFFFFFF
#define MAX_WINDOW_COUNT 8
#define DELETE_COOKIE_NAME "WM_DELETE_WINDOW"
#define PROTOCOLS_COOKIE_NAME "WM_PROTOCOLS"
#define REQUIRED_VULKAN_EXTENSION_COUNT 2

const char *const window_required_vulkan_extensions[REQUIRED_VULKAN_EXTENSION_COUNT] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_XCB_SURFACE_EXTENSION_NAME
};

typedef struct window_data_s {
    xcb_window_t handle;
    xcb_atom_t delete_atom;
    bool close_requested;
    u16 width;
    u16 height;
    void (*size_callback)(Window window, u32 width, u32 height);
} WindowData;

xcb_connection_t *window_xcb_connection = 0;
xcb_screen_t *window_xcb_screen = 0;
Window window_xcb_window_count = 0;
WindowData *window_xcb_windows_data = 0;

Window get_window_by_handle(xcb_window_t handle) {
    for (u32 i = 0; i < window_xcb_window_count; i++) {
        if (window_xcb_windows_data[i].handle == handle) {
            return i;
        }
    }
    return INVALID_WINDOW;
}

bool window_xcb_handle_event(xcb_generic_event_t *event) {
    if (event == NULL) {
        return false;
    }
    switch (event->response_type & ~0x80) {
        case 0: {
            xcb_generic_error_t *error = (xcb_generic_error_t *) event;
            printf("XCB Error: %d\n", error->error_code);
            break;
        }
        case XCB_CLIENT_MESSAGE: {
            xcb_client_message_event_t *client_message_event = (xcb_client_message_event_t *) event;
            Window window = get_window_by_handle(client_message_event->window);
            if (client_message_event->data.data32[0] == window_xcb_windows_data[window].delete_atom) {
                window_xcb_windows_data[window].close_requested = true;
            }
            break;
        }
        case XCB_RESIZE_REQUEST: {
            xcb_resize_request_event_t *resize_request_event = (xcb_resize_request_event_t *) event;
            Window window = get_window_by_handle(resize_request_event->window);
            if (resize_request_event->width > 0 && resize_request_event->height > 0) {
                u16 width = resize_request_event->width;
                u16 height = resize_request_event->height;
                window_xcb_windows_data[window].width = width;
                window_xcb_windows_data[window].height = height;
                if (window_xcb_windows_data[window].size_callback != NULL) {
                    window_xcb_windows_data[window].size_callback(window, width, height);
                }
            }
            break;
        }
    }
    free(event);
    return true;
}

void window_global_poll_events() {
    while (window_xcb_handle_event(xcb_poll_for_event(window_xcb_connection)));
}

void window_global_wait_events() {
    window_xcb_handle_event(xcb_wait_for_event(window_xcb_connection));
    window_global_poll_events();
}

void window_xcb_set_on_window_delete_interest(Window window) {
    xcb_intern_atom_cookie_t delete_cookie = xcb_intern_atom(
            window_xcb_connection,
            0,
            strlen(DELETE_COOKIE_NAME),
            DELETE_COOKIE_NAME
    );
    xcb_intern_atom_cookie_t protocols_cookie = xcb_intern_atom(
            window_xcb_connection,
            0,
            strlen(PROTOCOLS_COOKIE_NAME),
            PROTOCOLS_COOKIE_NAME
    );
    xcb_intern_atom_reply_t *delete_reply = xcb_intern_atom_reply(
            window_xcb_connection,
            delete_cookie,
            NULL
    );
    xcb_intern_atom_reply_t *protocols_reply = xcb_intern_atom_reply(
            window_xcb_connection,
            protocols_cookie,
            NULL
    );
    window_xcb_windows_data[window].delete_atom = delete_reply->atom;
    //wmProtocols = protocols_reply->atom;
    xcb_change_property(
            window_xcb_connection,
            XCB_PROP_MODE_REPLACE,
            window_xcb_windows_data[window].handle,
            protocols_reply->atom,
            4,
            32,
            1,
            &delete_reply->atom
    );
}

Window window_create(u16 width, u16 height, const char *title) {
    if (window_xcb_window_count == MAX_WINDOW_COUNT) {
        return INVALID_WINDOW;
    }
    if (window_xcb_window_count == 0) {
        int screen_number;
        window_xcb_connection = xcb_connect(NULL, &screen_number);
        const xcb_setup_t *xcb_setup = xcb_get_setup(window_xcb_connection);
        xcb_screen_iterator_t iter = xcb_setup_roots_iterator(xcb_setup);
        for (int i = 0; i < screen_number; ++i) {
            xcb_screen_next(&iter);
        }
        window_xcb_screen = iter.data;
        window_xcb_windows_data = malloc(sizeof(WindowData) * MAX_WINDOW_COUNT);
    }
    window_xcb_windows_data[window_xcb_window_count].handle = xcb_generate_id(window_xcb_connection);
    window_xcb_windows_data[window_xcb_window_count].close_requested = false;
    window_xcb_windows_data[window_xcb_window_count].width = width;
    window_xcb_windows_data[window_xcb_window_count].height = height;
    u32 eventMask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    u32 valueList[] = {window_xcb_screen->black_pixel, XCB_EVENT_MASK_RESIZE_REDIRECT};
    xcb_create_window(
            window_xcb_connection,
            window_xcb_screen->root_depth,
            window_xcb_windows_data[window_xcb_window_count].handle,
            window_xcb_screen->root,
            0, 0,
            width, height,
            0,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            window_xcb_screen->root_visual,
            eventMask,
            valueList
    );
    xcb_change_property(
            window_xcb_connection,
            XCB_PROP_MODE_REPLACE,
            window_xcb_windows_data[window_xcb_window_count].handle,
            XCB_ATOM_WM_NAME,
            XCB_ATOM_STRING,
            8,
            strlen(title),
            title
    );
    xcb_change_property(
            window_xcb_connection,
            XCB_PROP_MODE_REPLACE,
            window_xcb_windows_data[window_xcb_window_count].handle,
            XCB_ATOM_WM_CLASS,
            XCB_ATOM_STRING,
            8,
            strlen(title),
            title
    );
    window_xcb_set_on_window_delete_interest(window_xcb_window_count);
    xcb_map_window(window_xcb_connection, window_xcb_windows_data[window_xcb_window_count].handle);
    xcb_flush(window_xcb_connection);
    return window_xcb_window_count++;
}

void window_destroy(Window window) {
    if (window_xcb_connection == 0) {
        return;
    }
    xcb_unmap_window(window_xcb_connection, window_xcb_windows_data[window].handle);
    xcb_destroy_window(window_xcb_connection, window_xcb_windows_data[window].handle);
    window_xcb_window_count--;
    if (window_xcb_window_count == 0) {
        xcb_disconnect(window_xcb_connection);
        window_xcb_connection = 0;
        window_xcb_screen = 0;
        free(window_xcb_windows_data);
        window_xcb_windows_data = 0;
    }
}

bool window_is_close_requested(Window window) {
    return window_xcb_windows_data[window].close_requested;
}

u32 window_enumerate_required_vulkan_extensions(Window window, const char **extensions) {
    if (extensions) {
        for (int i = 0; i < REQUIRED_VULKAN_EXTENSION_COUNT; i++) {
            extensions[i] = window_required_vulkan_extensions[i];
        }
    }
    return REQUIRED_VULKAN_EXTENSION_COUNT;
}

VkResult window_create_vulkan_surface(Window window, VkInstance instance, VkSurfaceKHR *surface) {
    VkXcbSurfaceCreateInfoKHR surfaceCreateInfo = {};
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.pNext = NULL;
    surfaceCreateInfo.flags = 0;
    surfaceCreateInfo.connection = window_xcb_connection;
    surfaceCreateInfo.window = window_xcb_windows_data[window].handle;
    return vkCreateXcbSurfaceKHR(instance, &surfaceCreateInfo, NULL, surface);
}

void window_get_size_in_pixels(Window window, u32 *width, u32 *height) {
    *width = window_xcb_windows_data[window].width;
    *height = window_xcb_windows_data[window].height;
}

void window_set_size_callback(Window window, void (*callback)(Window window, u32 width, u32 height)) {
    window_xcb_windows_data[window].size_callback = callback;
}

#endif

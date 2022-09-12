#ifndef _WIN32

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <xcb/xcb.h>
#include "window_unix.h"

#define INVALID_WINDOW 0xFFFFFFFF
#define MAX_WINDOW_COUNT 8
#define DELETE_COOKIE_NAME "WM_DELETE_WINDOW"
#define PROTOCOLS_COOKIE_NAME "WM_PROTOCOLS"

typedef struct window_data_s {
    xcb_window_t handle;
    xcb_atom_t delete_atom;
    bool close_requested;
} WindowData;

xcb_connection_t *window_unix_connection = 0;
xcb_screen_t *window_unix_screen = 0;
Window window_unix_window_count = 0;
WindowData *window_unix_windows_data = 0;

Window get_window_by_handle(xcb_window_t handle) {
    for (u32 i = 0; i < window_unix_window_count; i++) {
        if (window_unix_windows_data[i].handle == handle) {
            return i;
        }
    }
    return INVALID_WINDOW;
}

bool window_unix_handle_event(xcb_generic_event_t *event) {
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
            if (client_message_event->data.data32[0] == window_unix_windows_data[window].delete_atom) {
                window_unix_windows_data[window].close_requested = true;
            }
            break;
        }
    }
    free(event);
    return true;
}

void window_global_poll_events() {
    while (window_unix_handle_event(xcb_poll_for_event(window_unix_connection)));
}

void window_global_wait_events() {
    window_unix_handle_event(xcb_wait_for_event(window_unix_connection));
    window_global_poll_events();
}

void window_unix_set_on_window_delete_interest(Window window) {
    xcb_intern_atom_cookie_t delete_cookie = xcb_intern_atom(
            window_unix_connection,
            0,
            strlen(DELETE_COOKIE_NAME),
            DELETE_COOKIE_NAME
    );
    xcb_intern_atom_cookie_t protocols_cookie = xcb_intern_atom(
            window_unix_connection,
            0,
            strlen(PROTOCOLS_COOKIE_NAME),
            PROTOCOLS_COOKIE_NAME
    );
    xcb_intern_atom_reply_t *delete_reply = xcb_intern_atom_reply(
            window_unix_connection,
            delete_cookie,
            NULL
    );
    xcb_intern_atom_reply_t *protocols_reply = xcb_intern_atom_reply(
            window_unix_connection,
            protocols_cookie,
            NULL
    );
    window_unix_windows_data[window].delete_atom = delete_reply->atom;
    //wmProtocols = protocols_reply->atom;
    xcb_change_property(
            window_unix_connection,
            XCB_PROP_MODE_REPLACE,
            window_unix_windows_data[window].handle,
            protocols_reply->atom,
            4,
            32,
            1,
            &delete_reply->atom
    );
}

Window window_create(u16 width, u16 height, const char *title) {
    if (window_unix_window_count == MAX_WINDOW_COUNT) {
        return INVALID_WINDOW;
    }
    if (window_unix_window_count == 0) {
        int screen_number;
        window_unix_connection = xcb_connect(NULL, &screen_number);
        const xcb_setup_t *xcb_setup = xcb_get_setup(window_unix_connection);
        xcb_screen_iterator_t iter = xcb_setup_roots_iterator(xcb_setup);
        for (int i = 0; i < screen_number; ++i) {
            xcb_screen_next(&iter);
        }
        window_unix_screen = iter.data;
        window_unix_windows_data = malloc(sizeof(WindowData) * MAX_WINDOW_COUNT);
    }
    window_unix_windows_data[window_unix_window_count].handle = xcb_generate_id(window_unix_connection);
    window_unix_windows_data[window_unix_window_count].close_requested = false;
    u32 eventMask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    u32 valueList[] = {window_unix_screen->black_pixel, 0};
    xcb_create_window(
            window_unix_connection,
            window_unix_screen->root_depth,
            window_unix_windows_data[window_unix_window_count].handle,
            window_unix_screen->root,
            0, 0,
            width, height,
            0,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            window_unix_screen->root_visual,
            eventMask,
            valueList
    );
    xcb_change_property(
            window_unix_connection,
            XCB_PROP_MODE_REPLACE,
            window_unix_windows_data[window_unix_window_count].handle,
            XCB_ATOM_WM_NAME,
            XCB_ATOM_STRING,
            8,
            strlen(title),
            title
    );
    window_unix_set_on_window_delete_interest(window_unix_window_count);
    xcb_map_window(window_unix_connection, window_unix_windows_data[window_unix_window_count].handle);
    xcb_flush(window_unix_connection);
    return window_unix_window_count++;
}

void window_destroy(Window window) {
    if (window_unix_connection == 0) {
        return;
    }
    xcb_unmap_window(window_unix_connection, window_unix_windows_data[window].handle);
    xcb_destroy_window(window_unix_connection, window_unix_windows_data[window].handle);
    window_unix_window_count--;
    if (window_unix_window_count == 0) {
        xcb_disconnect(window_unix_connection);
        window_unix_connection = 0;
        window_unix_screen = 0;
        free(window_unix_windows_data);
        window_unix_windows_data = 0;
    }
}

bool window_is_close_requested(Window window) {
    return window_unix_windows_data[window].close_requested;
}

#endif

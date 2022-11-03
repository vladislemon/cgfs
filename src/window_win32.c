#ifdef _WIN32

#include "window_win32.h"
#include <stdlib.h>
#include <stdio.h>
#include <windows.h>

#define INVALID_WINDOW 0xFFFFFFFF
#define MAX_WINDOW_COUNT 8

typedef struct window_data_s {
    HWND handle;
    WNDCLASSEX class;
    bool close_requested;
} WindowData;

HINSTANCE window_win32_module_handle = 0;
Window window_win32_window_count = 0;
WindowData *window_win32_windows_data = 0;

Window window_win32_get_window_by_handle(HWND handle) {
    for (u32 i = 0; i < window_win32_window_count; i++) {
        if (window_win32_windows_data[i].handle == handle) {
            return i;
        }
    }
    return INVALID_WINDOW;
}

bool window_win32_handle_message(MSG *message, int status) {
    if (status == 0) {
        return false;
    } else if (status == -1) {
        printf("WIN32 Error: %lu\n", GetLastError());
        return true;
    } else {
        TranslateMessage(message);
        DispatchMessage(message);
        return true;
    }
}

void window_global_poll_events() {
    MSG message;
    while (window_win32_handle_message(&message, PeekMessage(&message, NULL, 0, 0, PM_REMOVE)));
}

void window_global_wait_events() {
    MSG message;
    window_win32_handle_message(&message, GetMessage(&message, NULL, 0, 0));
    window_global_poll_events();
}

LRESULT CALLBACK window_win32_window_function(HWND handle, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CLOSE: {
            Window window = window_win32_get_window_by_handle(handle);
            window_win32_windows_data[window].close_requested = true;
            break;
        }
        case WM_PAINT: {
            ValidateRect(handle, NULL);
            break;
        }
        default: {
            return DefWindowProc(handle, message, wParam, lParam);
        }
    }
    return 0;
}

Window window_create(u16 width, u16 height, const char *title) {
    if (window_win32_window_count == MAX_WINDOW_COUNT) {
        return INVALID_WINDOW;
    }
    if (window_win32_window_count == 0) {
        window_win32_module_handle = GetModuleHandle(NULL);
        window_win32_windows_data = malloc(sizeof(WindowData) * MAX_WINDOW_COUNT);
    }
    WindowData *window_data = &window_win32_windows_data[window_win32_window_count];
    window_data->class.cbSize = sizeof(WNDCLASSEX);
    window_data->class.style = CS_HREDRAW | CS_VREDRAW;
    window_data->class.lpfnWndProc = window_win32_window_function;
    window_data->class.cbClsExtra = 0;
    window_data->class.cbWndExtra = 0;
    window_data->class.hInstance = window_win32_module_handle;
    window_data->class.hIcon = NULL;
    window_data->class.hCursor = LoadCursor(NULL, IDC_ARROW);
    window_data->class.hbrBackground = (HBRUSH) GetStockObject(BLACK_BRUSH);
    window_data->class.lpszMenuName = NULL;
    window_data->class.lpszClassName = title;
    window_data->class.hIconSm = NULL;
    ATOM class_atom = RegisterClassEx(&window_data->class);
    window_data->handle = CreateWindowEx(
            0,
            (LPCSTR) MAKELPARAM(class_atom, 0),
            title,
            WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
            CW_USEDEFAULT, CW_USEDEFAULT,
            width, height,
            NULL,
            NULL,
            window_win32_module_handle,
            NULL
    );
    window_data->close_requested = false;
    ShowWindow(window_data->handle, SW_SHOW);
    return window_win32_window_count++;
}

void window_destroy(Window window) {
    if (window >= window_win32_window_count) {
        return;
    }
    DestroyWindow(window_win32_windows_data[window].handle);
    window_win32_window_count--;
    if (window_win32_window_count == 0) {
        free(window_win32_windows_data);
        window_win32_windows_data = 0;
    }
}

bool window_is_close_requested(Window window) {
    if (window >= window_win32_window_count) {
        return false;
    }
    return window_win32_windows_data[window].close_requested;
}

#endif

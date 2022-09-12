#ifndef CGFS_WINDOW_H
#define CGFS_WINDOW_H

#ifdef _WIN32
#include "window_windows.h"
#else
#include "window_unix.h"
#endif

#include "types.h"

void window_global_wait_events();

Window window_create(u16 width, u16 height, const char *title);

void window_destroy(Window window);

bool window_is_close_requested(Window window);

#endif //CGFS_WINDOW_H

#ifndef CGFS_THREAD_H
#define CGFS_THREAD_H

#include "types.h"

#ifdef _WIN32
#include "thread_windows.h"
#else
#include "thread_unix.h"
#endif

Thread thread_create(void *(*entry_point)(void *), void *arg);

void thread_join(Thread thread, usize *result_holder);

void thread_exit(usize result);

#endif //CGFS_THREAD_H

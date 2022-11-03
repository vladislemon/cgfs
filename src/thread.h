#ifndef CGFS_THREAD_H
#define CGFS_THREAD_H

#include "types.h"

#ifdef _WIN32
#include "thread_win32.h"
#else
#include "thread_pthread.h"
#endif

Thread thread_create(void *(*entry_point)(void *), void *arg);

void thread_join(Thread thread, usize *result_holder);

void thread_exit(usize result);

void thread_sleep(u64 millis);

#endif //CGFS_THREAD_H

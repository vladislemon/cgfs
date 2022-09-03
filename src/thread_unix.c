#ifndef _WIN32

#include "thread_unix.h"
#include "types.h"

Thread thread_create(void *(*entry_point)(void *), void *arg) {
    Thread thread;
    int status = pthread_create(&thread, 0, entry_point, arg);
    if (status != 0) {
        return 0;
    }
    return thread;
}

void thread_join(Thread thread, usize *result_holder) {
    pthread_join(thread, (void **) result_holder);
}

void thread_exit(usize result) {
    pthread_exit((void *) result);
}

#endif

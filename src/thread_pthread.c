#ifndef _WIN32

#include <time.h>
#include "thread_pthread.h"
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

void thread_sleep(u64 millis) {
    struct timespec ts;
    ts.tv_sec = millis / 1000;
    ts.tv_nsec = (millis % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

#endif

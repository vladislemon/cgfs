#ifndef _WIN32

#include "mutex_pthread.h"

int mutex_init(Mutex *mutex) {
    return pthread_mutex_init(mutex, 0);
}

int mutex_lock(Mutex *mutex) {
    return pthread_mutex_lock(mutex);
}

int mutex_try_lock(Mutex *mutex) {
    return pthread_mutex_trylock(mutex);
}

int mutex_unlock(Mutex *mutex) {
    return pthread_mutex_unlock(mutex);
}

int mutex_destroy(Mutex *mutex) {
    return pthread_mutex_destroy(mutex);
}

#endif

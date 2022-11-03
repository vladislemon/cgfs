#ifdef _WIN32

#include "mutex_win32.h"

int mutex_init(Mutex *mutex) {
    InitializeCriticalSection(mutex);
    return 0;
}

int mutex_lock(Mutex *mutex) {
    EnterCriticalSection(mutex);
    return 0;
}

int mutex_try_lock(Mutex *mutex) {
    return !TryEnterCriticalSection(mutex);
}

int mutex_unlock(Mutex *mutex) {
    LeaveCriticalSection(mutex);
    return 0;
}

int mutex_destroy(Mutex *mutex) {
    DeleteCriticalSection(mutex);
    return 0;
}

#endif

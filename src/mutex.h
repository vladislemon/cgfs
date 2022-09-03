#ifndef CGFS_MUTEX_H
#define CGFS_MUTEX_H

#include "thread.h"

#ifdef _WIN32
#include "mutex_windows.h"
#else
#include "mutex_unix.h"
#endif

int mutex_init(Mutex *mutex);

int mutex_lock(Mutex *mutex);

int mutex_try_lock(Mutex *mutex);

int mutex_unlock(Mutex *mutex);

int mutex_destroy(Mutex *mutex);

#endif //CGFS_MUTEX_H

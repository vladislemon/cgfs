#include "starter.h"
#include <stdio.h>
#include "socket.h"
#include "thread.h"
#include "mutex.h"
#include "window.h"

const char *message = "Some message";

Mutex mutex;

void *second_thread_entry_point(void *arg) {
    printf("%s\n", (const char *) arg);
    mutex_lock(&mutex);
    printf("Mutex locked in second thread\n");
    thread_sleep(1000);
    mutex_unlock(&mutex);
    printf("Mutex unlocked in second thread\n");
    thread_exit(12345);
}

int cgfs_start() {
    socket_global_init();
    Socket sock = socket_create(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct addrinfo *address_info;
    socket_get_address_info("google.com", "80", 0, &address_info);
    socket_connect(sock, address_info->ai_addr, (int) address_info->ai_addrlen);
    socket_free_address_info(address_info);
    socket_shutdown(sock, SHUT_RDWR);
    socket_close(sock);
    socket_global_destroy();

    mutex_init(&mutex);
    Thread thread = thread_create(second_thread_entry_point, (void *) message);

    mutex_lock(&mutex);
    printf("Mutex locked in main thread\n");
    thread_sleep(1000);
    mutex_unlock(&mutex);
    printf("Mutex unlocked in main thread\n");

    usize thread_result;
    thread_join(thread, &thread_result);
    printf("%llu\n", thread_result);

    mutex_destroy(&mutex);

    Window window = window_create(800, 600, "Hello from window");
    while (!window_is_close_requested(window)) {
        window_global_wait_events();
    }
    window_destroy(window);

    return 0;
}

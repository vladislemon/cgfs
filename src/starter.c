#include "starter.h"
#include "socket.h"
#include "thread.h"
#include "mutex.h"
#include "window.h"
#include "renderer.h"
#include "file.h"
#include <stdio.h>
#include <stdlib.h>

const char *message = "Some message";

Mutex mutex;

typedef struct cgfs_global_state_s {
    Window window;
    Renderer renderer;
} CgfsGlobalState;

static CgfsGlobalState cgfs_global_state;

void *second_thread_entry_point(void *arg) {
    printf("%s\n", (const char *) arg);
    mutex_lock(&mutex);
    printf("Mutex locked in second thread\n");
    thread_sleep(1000);
    mutex_unlock(&mutex);
    printf("Mutex unlocked in second thread\n");
    thread_exit(12345);
    return NULL;
}

void test_socket() {
    socket_global_init();
    Socket sock = socket_create(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct addrinfo *address_info;
    socket_get_address_info("google.com", "80", 0, &address_info);
    socket_connect(sock, address_info->ai_addr, (int) address_info->ai_addrlen);
    socket_free_address_info(address_info);
    socket_shutdown(sock, SHUT_RDWR);
    socket_close(sock);
    socket_global_destroy();
}

void test_concurrency() {
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
}

usize shader_data_from_file(const char *path, u32 **data) {
    usize length;
    if (file_read_all_binary(path, &length, NULL) != 0) {
        return 0;
    }
    *data = malloc((length / sizeof(u32) + 1) * sizeof(u32));
    if (file_read_all_binary(path, &length, (u8 *) *data) != 0) {
        free(*data);
        return 0;
    }
    return length;
}

Renderer create_renderer(Window window) {
    u32 *vertex_shader_spv;
    usize vertex_shader_length = shader_data_from_file("shaders/shader.vert.spv", &vertex_shader_spv);
    if (vertex_shader_length == 0) {
        return -1;
    }
    u32 *fragment_shader_spv;
    usize fragment_shader_length = shader_data_from_file("shaders/shader.frag.spv", &fragment_shader_spv);
    if (fragment_shader_length == 0) {
        return -1;
    }
    Renderer renderer = renderer_create(
            window,
            vertex_shader_length,
            vertex_shader_spv,
            fragment_shader_length,
            fragment_shader_spv
    );
    free(fragment_shader_spv);
    free(vertex_shader_spv);
    return renderer;
}

void size_callback(Window window, u32 width, u32 height) {
    renderer_reload(cgfs_global_state.renderer);
}

int cgfs_start() {
    cgfs_global_state.window = window_create(800, 600, "cgfs");
    cgfs_global_state.renderer = create_renderer(cgfs_global_state.window);
    printf("Renderer: %d\n", cgfs_global_state.renderer);
    window_set_size_callback(cgfs_global_state.window, size_callback);
    while (!window_is_close_requested(cgfs_global_state.window)) {
        window_global_wait_events();
        renderer_draw_frame(cgfs_global_state.renderer);
    }
    renderer_destroy(cgfs_global_state.renderer);
    window_destroy(cgfs_global_state.window);

    return 0;
}

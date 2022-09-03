#include <stdio.h>
#include "socket.h"

int main() {
#ifdef _WIN32
    printf("Hello, World!\n");
#endif
    socket_global_init();
    Socket sock = socket_create(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct addrinfo *address_info;
    socket_get_address_info("google.com", "80", 0, &address_info);
    socket_connect(sock, address_info->ai_addr, address_info->ai_addrlen);
    socket_free_address_info(address_info);
    socket_shutdown(sock, SHUT_RDWR);
    socket_close(sock);
    socket_global_destroy();
    return 0;
}

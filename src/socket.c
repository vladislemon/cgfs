#include "socket.h"

int socket_global_init() {
#ifdef _WIN32
    WSADATA wsaData;
    WORD versionRequested = MAKEWORD(1, 1);
    int status = WSAStartup(versionRequested, &wsaData);
    if (status != 0) {
        return status;
    }
    if (wsaData.wVersion != versionRequested) {
        WSACleanup();
        return -1;
    }
#endif
    return 0;
}

int socket_global_destroy() {
#ifdef _WIN32
    return WSACleanup();
#else
    return 0;
#endif
}

Socket socket_create(int domain, int type, int protocol) {
    return socket(domain, type, protocol);
}

int socket_bind(Socket sock, const struct sockaddr *addr, int addr_length) {
    return bind(sock, addr, addr_length);
}

int socket_get_address_info(const char *hostname, const char *service, const struct addrinfo *hints,
                            struct addrinfo **result) {
    return getaddrinfo(hostname, service, hints, result);
}

void socket_free_address_info(struct addrinfo *address_info) {
    freeaddrinfo(address_info);
}

int socket_connect(Socket sock, const struct sockaddr *addr, int addr_length) {
    return connect(sock, addr, addr_length);
}

int socket_send(Socket sock, const void *buffer, int length, int flags) {
    return send(sock, buffer, length, flags);
}

int socket_receive(Socket sock, void *buffer, int length, int flags) {
    return recv(sock, buffer, length, flags);
}

int socket_shutdown(Socket sock, int how) {
    return shutdown(sock, how);
}

int socket_close(Socket sock) {
#ifdef _WIN32
    return closesocket(sock);
#else
    return close(sock);
#endif
}

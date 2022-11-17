#ifndef CGFS_SOCKET_H
#define CGFS_SOCKET_H

#ifdef _WIN32
/* See http://stackoverflow.com/questions/12765743/getaddrinfo-on-win32 */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501  /* Windows XP. */
#endif

#include <winsock2.h>
#include <ws2tcpip.h>

typedef SOCKET Socket;

enum {
    SHUT_RD = SD_RECEIVE,
#define SHUT_RD SHUT_RD
    SHUT_WR = SD_SEND,
#define SHUT_WR SHUT_WR
    SHUT_RDWR = SD_BOTH,
#define SHUT_RDWR SHUT_RDWR
};

#else
/* Assume that any non-Windows platform uses POSIX-style sockets instead. */
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>  /* Needed for getaddrinfo() and freeaddrinfo() */
#include <unistd.h> /* Needed for close() */

typedef int Socket;
#endif

int socket_global_init();

int socket_global_destroy();

Socket socket_create(int domain, int type, int protocol);

int socket_get_address_info(const char *hostname, const char *service, const struct addrinfo *hints,
                            struct addrinfo **result);

void socket_free_address_info(struct addrinfo *address_info);

int socket_bind(Socket sock, const struct sockaddr *addr, int addr_length);

int socket_connect(Socket sock, const struct sockaddr *addr, int addr_length);

int socket_send(Socket sock, const void *buffer, int length, int flags);

int socket_receive(Socket sock, void *buffer, int length, int flags);

int socket_shutdown(Socket sock, int how);

int socket_close(Socket sock);

#endif //CGFS_SOCKET_H

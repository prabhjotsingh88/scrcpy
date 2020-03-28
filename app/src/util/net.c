#include "net.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <SDL2/SDL_platform.h>

#include "config.h"
#include "common.h"
#include "log.h"

#ifdef __WINDOWS__
# include <io.h>
# include <winsock2.h>
  typedef int socklen_t;
#else
# include <sys/select.h>
# include <sys/socket.h>
# include <sys/types.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <unistd.h>
# define SOCKET_ERROR -1
  typedef struct sockaddr_in SOCKADDR_IN;
  typedef struct sockaddr SOCKADDR;
  typedef struct in_addr IN_ADDR;
#endif

socket_t
net_connect(uint32_t addr, uint16_t port) {
    socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        perror("socket");
        return INVALID_SOCKET;
    }

    SOCKADDR_IN sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(addr);
    sin.sin_port = htons(port);

    if (connect(sock, (SOCKADDR *) &sin, sizeof(sin)) == SOCKET_ERROR) {
        perror("connect");
        net_close(sock);
        return INVALID_SOCKET;
    }

    return sock;
}

socket_t
net_listen(uint32_t addr, uint16_t port, int backlog) {
    socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        perror("socket");
        return INVALID_SOCKET;
    }

    int reuse = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const void *) &reuse,
                   sizeof(reuse)) == -1) {
        perror("setsockopt(SO_REUSEADDR)");
    }

    SOCKADDR_IN sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(addr); // htonl() harmless on INADDR_ANY
    sin.sin_port = htons(port);

    if (bind(sock, (SOCKADDR *) &sin, sizeof(sin)) == SOCKET_ERROR) {
        perror("bind");
        net_close(sock);
        return INVALID_SOCKET;
    }

    if (listen(sock, backlog) == SOCKET_ERROR) {
        perror("listen");
        net_close(sock);
        return INVALID_SOCKET;
    }

    return sock;
}

socket_t
net_accept(socket_t server_socket) {
    SOCKADDR_IN csin;
    socklen_t sinsize = sizeof(csin);
    return accept(server_socket, (SOCKADDR *) &csin, &sinsize);
}

ssize_t
net_recv(socket_t socket, void *buf, size_t len) {
    return recv(socket, buf, len, 0);
}

ssize_t
net_recv_all(socket_t socket, void *buf, size_t len) {
    return recv(socket, buf, len, MSG_WAITALL);
}

ssize_t
net_send(socket_t socket, const void *buf, size_t len) {
    return send(socket, buf, len, 0);
}

ssize_t
net_send_all(socket_t socket, const void *buf, size_t len) {
    ssize_t w = 0;
    while (len > 0) {
        w = send(socket, buf, len, 0);
        if (w == -1) {
            return -1;
        }
        len -= w;
        buf = (char *) buf + w;
    }
    return w;
}

bool
net_shutdown(socket_t socket, int how) {
    return !shutdown(socket, how);
}

bool
net_init(void) {
#ifdef __WINDOWS__
    WSADATA wsa;
    int res = WSAStartup(MAKEWORD(2, 2), &wsa) < 0;
    if (res < 0) {
        LOGC("WSAStartup failed with error %d", res);
        return false;
    }
#endif
    return true;
}

void
net_cleanup(void) {
#ifdef __WINDOWS__
    WSACleanup();
#endif
}

bool
net_close(socket_t socket) {
#ifdef __WINDOWS__
    return !closesocket(socket);
#else
    return !close(socket);
#endif
}

bool
net_select_interruptible(int fd, int fd_intr) {
    fd_set rfds;

    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    FD_SET(fd_intr, &rfds);

    int nfds = MAX(fd, fd_intr) + 1;

    // use select() because it's available on supported platforms
    int r = select(nfds, &rfds, NULL, NULL, NULL);
    if (r == -1) {
        // failure
        return false;
    }
    assert(r > 0);
    if (FD_ISSET(fd_intr, &rfds)) {
        // interrupted is set
        return false;
    }

    assert(FD_ISSET(fd, &rfds));
    return true;
}

bool
net_pipe(int fds[static 2]) {
#ifdef __WINDOWS__
    return !_pipe(fds, 4096, 0);
#else
    return !pipe(fds);
#endif
}

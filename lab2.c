#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT        8080
#define BACKLOG     16
#define BUF_SIZE    1024

static volatile sig_atomic_t was_sighup = 0;
static volatile sig_atomic_t terminate  = 0;

static void sighup_handler(int signo)
{
    (void)signo;
    was_sighup = 1;
}

static int create_server_socket(int port)
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        return -1;
    }

    int yes = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,
                   &yes, sizeof(yes)) == -1) {
        perror("setsockopt(SO_REUSEADDR)");
        close(server_fd);
        return -1;
    }

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family      = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port        = htons((uint16_t)port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == -1) {
        perror("bind");
        close(server_fd);
        return -1;
    }
    if (listen(server_fd, BACKLOG) == -1) {
        perror("listen");
        close(server_fd);
        return -1;
    }
    return server_fd;
}

int main(void)
{
    int server_fd = create_server_socket(PORT);
    if (server_fd == -1)
        return EXIT_FAILURE;

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = sighup_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    if (sigaction(SIGHUP, &action, NULL) == -1) {
        perror("sigaction");
        close(server_fd);
        return EXIT_FAILURE;
    }

    sigset_t blocked_mask, original_mask;
    sigemptyset(&blocked_mask);
    sigaddset(&blocked_mask, SIGHUP);
    if (sigprocmask(SIG_BLOCK, &blocked_mask, &original_mask) == -1) {
        perror("sigprocmask");
        close(server_fd);
        return EXIT_FAILURE;
    }

    int client_fd = -1;

    while (!terminate) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);

        int max_fd = server_fd;
        if (client_fd != -1) {
            FD_SET(client_fd, &read_fds);
            if (client_fd > max_fd)
                max_fd = client_fd;
        }

        int ready = pselect(max_fd + 1, &read_fds, NULL, NULL,
                            NULL, &original_mask);

        if (ready == -1) {
            if (errno == EINTR) {
                if (was_sighup) {
                    was_sighup = 0;
                    printf("[signal] SIGHUP received\n");
                    fflush(stdout);
                }
                continue;
            }
            perror("pselect");
            break;
        }

        if (was_sighup) {
            was_sighup = 0;
            printf("[signal] SIGHUP received\n");
            fflush(stdout);
        }

        if (FD_ISSET(server_fd, &read_fds)) {
            int new_fd = accept(server_fd, NULL, NULL);
            if (new_fd == -1) {
                if (errno != EINTR)
                    perror("accept");
            } else if (client_fd == -1) {
                client_fd = new_fd;
                printf("[accept] new client connected, fd=%d (kept open)\n",
                       client_fd);
            } else {
                printf("[accept] extra client fd=%d closed immediately\n",
                       new_fd);
                close(new_fd);
            }
            fflush(stdout);
        }

        if (client_fd != -1 && FD_ISSET(client_fd, &read_fds)) {
            char buffer[BUF_SIZE];
            ssize_t received = recv(client_fd, buffer, sizeof(buffer), 0);

            if (received > 0) {
                printf("[recv] received %zd bytes\n", received);
                fflush(stdout);
            } else if (received == 0) {
                printf("[recv] client closed connection\n");
                fflush(stdout);
                close(client_fd);
                client_fd = -1;
            } else {
                if (errno == EINTR)
                    continue;
                perror("recv");
                close(client_fd);
                client_fd = -1;
            }
        }
    }

    if (client_fd != -1)
        close(client_fd);
    close(server_fd);
    if (sigprocmask(SIG_SETMASK, &original_mask, NULL) == -1)
        perror("sigprocmask(restore)");

    return EXIT_SUCCESS;
}

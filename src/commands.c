#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <assert.h>

#include "commands.h"

void setup_server_socket(struct listen_sock *s, int port) {
    if(!(s->addr = malloc(sizeof(struct sockaddr_in)))) {
        perror("malloc");
        exit(1);
    }
    // Allow sockets across machines.
    s->addr->sin_family = AF_INET;
    // The port the process will listen on.
    s->addr->sin_port = htons(port);
    // Clear this field; sin_zero is used for padding for the struct.
    memset(&(s->addr->sin_zero), 0, 8);
    // Listen on all network interfaces.
    s->addr->sin_addr.s_addr = INADDR_ANY;

    s->sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (s->sock_fd < 0) {
        perror("server socket");
        exit(1);
    }

    // Make sure we can reuse the port immediately after the
    // server terminates. Avoids the "address in use" error
    int on = 1;
    int status = setsockopt(s->sock_fd, SOL_SOCKET, SO_REUSEADDR,
        (const char *) &on, sizeof(on));
    if (status < 0) {
        perror("setsockopt");
        exit(1);
    }

    // Bind the selected port to the socket.
    if (bind(s->sock_fd, (struct sockaddr *)s->addr, sizeof(*(s->addr))) < 0) {
        perror("server: bind");
        close(s->sock_fd);
        exit(1);
    }

    // Announce willingness to accept connections on this socket.
    if (listen(s->sock_fd, MAX_BACKLOG) < 0) {
        perror("server: listen");
        close(s->sock_fd);
        exit(1);
    }
}

/* Insert helper functions from last week here. */

int find_network_newline(const char *buf, int inbuf) {
    for (int i = 0; i < inbuf - 1; i++) {
        if (buf[i] == '\r' && buf[i + 1] == '\n') {
            return i + 2;
        }
    }
    return -1;
}

int read_from_socket(int sock_fd, char *buf, int *inbuf) {
    int bytes_read = read(sock_fd, buf + *inbuf, BUF_SIZE - *inbuf);

    if (bytes_read < 0) {
        perror("read");
        return -1;
    }
    if (bytes_read == 0) {
        return 1;
    }

    *inbuf += bytes_read;
    int newline_index = find_network_newline(buf, *inbuf);

    if (newline_index != -1) {
        return 0;
    }

    if (*inbuf >= BUF_SIZE) {
        return -1;
    }

    return 2;
}

int get_message(char **dst, char *src, int *inbuf) {
    int newline_index = find_network_newline(src, *inbuf);
    if (newline_index == -1) {
        return 1;
    }

    *dst = malloc(newline_index + 1);
    if (!*dst) {
        perror("malloc");
        return 1;
    }

    memcpy(*dst, src, newline_index - 2);
    (*dst)[newline_index - 2] = '\0';

    *inbuf -= newline_index;
    memmove(src, src + newline_index, *inbuf);

    return 0;
}

/* Helper function to be completed for this week. */
int write_to_socket(int sock_fd, char *buf, int len) {
    int total_written = 0;
    while (total_written < len) {
        ssize_t bytes_written = write(sock_fd, buf + total_written, len - total_written);
        if (bytes_written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return 1;
        } else if (bytes_written == 0) {
            return 2;
        }
        total_written += bytes_written;
    }
    return 0;
}

//HELPER FUNCTIONS
int write_buf_to_client(struct client_sock *c, char *buf, int len) {
    if (len < 2 || buf[len - 1] != '\n' || buf[len - 2] != '\r') {
        size_t required_size = len + 2;
        if (required_size > BUF_SIZE) {
            perror("Buffer overflow.\n");
            return 1;
        }
        buf[len++] = '\r';
        buf[len++] = '\n';
    }
    return write_to_socket(c->sock_fd, buf, len);
}

int remove_client(struct client_sock **curr, struct client_sock **clients) {
    if (!curr || !*curr || !clients || !*clients) return -1; // Empty list or invalid client

    struct client_sock *prev = NULL, *temp = *clients;
    while (temp != NULL && temp != *curr) {
        prev = temp;
        temp = temp->next;
    }

    if (temp == NULL) {
        return -1;
    }

    if (prev == NULL) {
        *clients = temp->next;
    } else {
        prev->next = temp->next;
    }
    *curr = temp->next;

    free(temp->username);
    close(temp->sock_fd);
    free(temp);
    return 0;
}

int read_from_client(struct client_sock *curr) {
    return read_from_socket(curr->sock_fd, curr->buf, &(curr->inbuf));
}

int set_username(struct client_sock *curr) {
    char *name = NULL;
    int reciever = get_message(&name, curr->buf, &(curr->inbuf));
    if (reciever == -1) {
        return -1;
    }
    if (strchr(name, ' ') != NULL || strchr(name, '\n') != NULL || strchr(name, '\r') != NULL) {
        free(name);
        return -1;
    }

    curr->username = malloc(strlen(name) + 1);
    if (curr->username == NULL) {
        free(name);
        return -1;
    }
    strcpy(curr->username, name);
    free(name);
    return 0;
}

//SERVER INFO
int sigint_received = 0;

void sigint_handler(int code) {
    sigint_received = 1;
}

/*
 * Wait for and accept a new connection.
 * Return -1 if the accept call failed.
 */
int accept_connection(int fd, struct client_sock **clients) {
    struct sockaddr_in peer;
    unsigned int peer_len = sizeof(peer);
    peer.sin_family = AF_INET;

    int num_clients = 0;
    struct client_sock *curr = *clients;
    while (curr != NULL && num_clients < MAX_CONNECTIONS && curr->next != NULL) {
        curr = curr->next;
        num_clients++;
    }

    int client_fd = accept(fd, (struct sockaddr *)&peer, &peer_len);

    if (client_fd < 0) {
        perror("server: accept");
        close(fd);
        exit(1);
    }

    if (num_clients == MAX_CONNECTIONS) {
        close(client_fd);
        return -1;
    }

    struct client_sock *newclient = malloc(sizeof(struct client_sock));
    newclient->sock_fd = client_fd;
    newclient->inbuf = newclient->state = 0;
    newclient->username = NULL;
    newclient->next = NULL;
    memset(newclient->buf, 0, BUF_SIZE);
    if (*clients == NULL) {
        *clients = newclient;
    }
    else {
        curr->next = newclient;
    }

    return client_fd;
}

/*
 * Close all sockets, free memory, and exit with specified exit status.
 */
void clean_exit(struct listen_sock s, struct client_sock *clients, int exit_status) {
    struct client_sock *tmp;
    while (clients) {
        tmp = clients;
        close(tmp->sock_fd);
        clients = clients->next;
        free(tmp->username);
        free(tmp);
    }
    close(s.sock_fd);
    free(s.addr);
    exit(exit_status);
}
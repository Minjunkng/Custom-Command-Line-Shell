#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "builtins.h"
#include "io_helpers.h"
#include "variables.h"
#include "commands.h"

extern char **var_keys, **var_vals;
extern int *var_size;

// ====== Command execution =====

/* Return: index of builtin or -1 if cmd doesn't match a builtin
 */
bn_ptr check_builtin(const char *cmd) {
    ssize_t cmd_num = 0;

    char *equal_sign = strchr(cmd, '=');
    if (equal_sign != NULL && equal_sign != cmd && *(equal_sign + 1) != '\0') {
        return bn_assign;}

    while (cmd_num < BUILTINS_COUNT &&
           strncmp(BUILTINS[cmd_num], cmd, MAX_STR_LEN) != 0) {
        cmd_num += 1;
    }
    return BUILTINS_FN[cmd_num];
}


// ===== Builtins =====

/* Prereq: tokens is a NULL terminated sequence of strings.
 * Return 0 on success and -1 on error ... but there are no errors on echo. 
 */
ssize_t bn_echo(char **tokens) {
    if (check_for_pipe(tokens) != -1) {
        return 0;
    }

    ssize_t index = 1;

    if (tokens[index] != NULL) {
        char *message = process_token(tokens[index]);
        display_message(message);
        free(message);
        index++;
    }

    while (tokens[index] != NULL) {
        display_message(" ");
        char *message = process_token(tokens[index]);
        display_message(message);
        free(message);
        index++;
    }

    display_message("\n");
    return 0;
}

ssize_t bn_assign(char **tokens) {
  if (check_for_pipe(tokens) != -1) {
        return 0;
    }

  if (tokens[1] == NULL) {
       const char *equal_sign = strchr(tokens[0], '=');

       char before[MAX_STR_LEN];
       char after[MAX_STR_LEN];


       strncpy(before, tokens[0], equal_sign - tokens[0]);
       before[equal_sign - tokens[0]] = '\0';

       strcpy(after, equal_sign + 1);
       assign_variables(&var_keys, &var_vals, &var_size, before, after);
       return 0;
  }
  return -1;
}

char *process_token(char *token) {
    char *ptr = token;
    char output[MAX_STR_LEN] = "";

    while (*ptr != '\0') {
        char *dollar_pos = strchr(ptr, '$');

        if (!dollar_pos) {
            strncat(output, ptr, MAX_STR_LEN - 1);
            break;
        }

        if (dollar_pos > ptr) {
            char temp[128];
            strncpy(temp, ptr, dollar_pos - ptr);
            temp[dollar_pos - ptr] = '\0';
            strncat(output, temp, MAX_STR_LEN - strlen(output) - 1);
        }

        ptr = dollar_pos;
        int dollar_count = 0;
        while (*ptr == '$') {
            dollar_count++;
            ptr++;
        }

        char *next_dollar = strchr(ptr, '$');
        size_t var_len = next_dollar ? (size_t)(next_dollar - ptr) : strlen(ptr);

        char var_name[128];
        if (var_len >= sizeof(var_name)) var_len = sizeof(var_name) - 1;
        strncpy(var_name, ptr, var_len);
        var_name[var_len] = '\0';

        if (var_name[0] == '\0') {
            for (int i = 0; i < dollar_count; i++) {
                strncat(output, "$", MAX_STR_LEN - strlen(output) - 1);
            }
        } else {
            for (int i = 0; i < dollar_count - 1; i++) {
                strncat(output, "$", MAX_STR_LEN - strlen(output) - 1);
            }
        }

        strncat(output, print_variables(&var_keys, &var_vals, &var_size, var_name), MAX_STR_LEN - strlen(output) - 1);
        ptr += var_len;

    }

    size_t length = strlen(output) + 1;

    char *output1 = (char *)malloc(length);
    if (output1 == NULL) {
        display_error("Memory allocation failed\n", "");
        return NULL;
    }

    strcpy(output1, output);
    return output1;
}

ssize_t bn_ls(char **tokens) {
    if (check_for_pipe(tokens) != -1) {
        return 0;
    }
    printf("I am called hehe\n");

    char *path = ".";
    char *filter = NULL;
    int recursive = 0;
    int depth = -1;

    for (int i = 1; tokens[i] != NULL; i++) {
        if (strcmp(tokens[i], "--f") == 0) {
            if (tokens[i + 1] == NULL) {
                display_error("ERROR: Missing argument for ", "--f");
                return -1;
            }
            filter = tokens[i + 1];
            i++;
        } else if (strcmp(tokens[i], "--rec") == 0) {
            recursive = 1;
        } else if (strcmp(tokens[i], "--d") == 0) {
            if (tokens[i + 1] == NULL || atoi(tokens[i + 1]) <= 0) {
                display_error("ERROR: Invalid depth value", "");
                return -1;
            }
            depth = atoi(tokens[i + 1]);
            i++;
        } else if (strcmp(path, ".") == 0) {
            path = tokens[i];
        } else {
            display_error("ERROR: Too many arguments: ls takes a single path", "");
            return -1;
        }
    }

    if (depth != -1 && !recursive) {
        display_error("ERROR: --d provided without --rec", "");
        return -1;
    }

    char *processed_path = process_token((char *)path);
    if (!processed_path) {
        return -1;
    }

    const char *final_path = processed_path;

    if (strcmp(final_path, "...") == 0) {
        path = "../..";
        ssize_t result = list_directory(path, filter, recursive, depth, 0);
    	free(processed_path);
    	return result;
    } else if (strcmp(final_path, "....") == 0) {
        path = "../../..";
        ssize_t result = list_directory(path, filter, recursive, depth, 0);
    	free(processed_path);
    	return result;
    }

    ssize_t result = list_directory(processed_path, filter, recursive, depth, 0);
    free(processed_path);
    return result;
}

ssize_t list_directory(const char *path, const char *filter, int recursive, int depth, int current_depth) {
    DIR *dir = opendir(path);
    if (!dir) {
        display_error("ERROR: Invalid path:", "");
        return -1;
    }

    struct dirent *entry;
    struct stat path_stat;

    if (!filter) {
        display_message(".\n");
        display_message("..\n");
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char new_path[1024];
        snprintf(new_path, sizeof(new_path), "%s/%s", path, entry->d_name);

        if (lstat(new_path, &path_stat) == 0) {

            int is_hidden = (entry->d_name[0] == '.');

            if (is_hidden && (!filter || strchr(filter, '/') == NULL)) {
                if (!S_ISDIR(path_stat.st_mode) || strcmp(path, new_path) != 0) {
                    continue;
                }
            }

            if (!filter || strstr(entry->d_name, filter)) {
                display_message(entry->d_name);
                display_message("\n");
            }

            if (recursive && S_ISDIR(path_stat.st_mode) && (depth == -1 || current_depth < depth)) {
                char *processed_path = process_token(new_path);
                if (!processed_path) {
                    closedir(dir);
                    return -1;
                }

                list_directory(processed_path, filter, recursive, depth, current_depth + 1);
                free(processed_path);
            }
        }
    }
    closedir(dir);
    return 0;
}

ssize_t bn_cd(char **tokens) {
    if (check_for_pipe(tokens) != -1) {
        return 0;
    }

    if (check_length(tokens, 2) == 2) {
      display_error("ERROR: Too many arguments: cd takes a single path", "");
      return -1;
    }

    if (tokens[1] == NULL) {
        display_error("ERROR: Missing path", "");
        return -1;
    }

    char *path = tokens[1];

    char *processed_path = process_token((char *)path);
    if (!processed_path) {
        free(processed_path);
        return -1;
    }

    const char *final_path = processed_path;

    if (strcmp(final_path, "...") == 0) {
        path = "../..";
    } else if (strcmp(final_path, "....") == 0) {
        path = "../../..";
    }

    if (chdir(path) != 0) {
        display_error("ERROR: Invalid path:", path);
        free(processed_path);
        return -1;
    }
    free(processed_path);
    return 0;
}

ssize_t bn_cat(char **tokens) {
    if (check_for_pipe(tokens) != -1) {
        return 0;
    }

    int fd = STDIN_FILENO;
    char *processed_path = NULL;
    if (check_length(tokens, 2) == 2) {
      display_error("ERROR: Too many arguments: cat takes a single file", "");
      return -1;
    }

    if (tokens[1] != NULL) {
        processed_path = process_token(tokens[1]);
        if (!processed_path) return -1;

        fd = open(processed_path, O_RDONLY);
        if (fd < 0) {
            display_error("ERROR: Cannot open file", "");
            free(processed_path);
            return -1;
        }
    } else if (isatty(STDIN_FILENO)) {
        display_error("ERROR: No input source provided", "");
        return -1;
    }

    char buffer[1024];
    ssize_t bytes;
    while ((bytes = read(fd, buffer, sizeof(buffer))) > 0) {
        write(STDOUT_FILENO, buffer, bytes);
    }

    if (fd != STDIN_FILENO) {
        close(fd);
        free(processed_path);
    }
    return 0;
}

ssize_t bn_wc(char **tokens) {
    if (check_for_pipe(tokens) != -1) {
        return 0;
    }

    int fd = STDIN_FILENO;
    char *processed_path = NULL;

    if (check_length(tokens, 2) == 2) {
      display_error("ERROR: Too many arguments: wc takes a single file", "");
      return -1;
    }

     if (tokens[1] != NULL) {
        processed_path = process_token(tokens[1]);
        if (!processed_path) return -1;

        fd = open(processed_path, O_RDONLY);
        if (fd < 0) {
            display_error("ERROR: Cannot open file", "");
            free(processed_path);
            return -1;
        }
    } else if (isatty(STDIN_FILENO)) {
        display_error("ERROR: No input source provided", "");
        return -1;
    }

    int words = 0, characters = 0, newlines = 0;
    char buffer[1024];
    ssize_t bytes;
    int in_word = 0;

    while ((bytes = read(fd, buffer, sizeof(buffer))) > 0) {
        for (ssize_t i = 0; i < bytes; i++) {
            characters++;
            if (buffer[i] == '\n') newlines++;
            if (buffer[i] == ' ' || buffer[i] == '\n' || buffer[i] == '\t' || buffer[i] == '\r') {
                if (in_word) {
                    words++;
                    in_word = 0;
                }
            } else {
                in_word = 1;
            }
        }
    }
    if (in_word) words++;

    if (fd != STDIN_FILENO) {
        close(fd);
        free(processed_path);
    }

    char buffer1[MAX_STR_LEN];
    snprintf(buffer1, MAX_STR_LEN, "word count %d\ncharacter count %d\nnewline count %d\n", words, characters, newlines);

    display_message(buffer1);
    return 0;
}

//Returns the number of counters if its above the limit, else returns 0
ssize_t check_length(char **tokens, int index) {
  int count = 0;
    while (tokens[count] != NULL) {
        count++;
        if (count > index) {
            return count - 1;
        }
    }
    return 0;
}


//MILESTONE 4 CODE
int check_for_pipe(char **tokens) {
    int pipe_index = -1;

    // Find the pipe index
    for (int i = 0; tokens[i] != NULL; i++) {
        if (strcmp(tokens[i], "|") == 0) {
            pipe_index = i;
            break;
        }
    }

    if (pipe_index != -1) {
        char *left_cmd[pipe_index + 1];
        for (int i = 0; i < pipe_index; i++) {
            left_cmd[i] = tokens[i];
        }
        left_cmd[pipe_index] = NULL;


        int right_size = 0;
        for (int i = pipe_index + 1; tokens[i] != NULL; i++) {
            right_size++;
        }

        char *right_cmd[right_size + 1];
        int right_index = 0;
        for (int i = pipe_index + 1; tokens[i] != NULL; i++) {
            right_cmd[right_index++] = tokens[i];
        }
        right_cmd[right_index] = NULL;

        execute_pipe(left_cmd, right_cmd);
        return pipe_index;
    }
    return -1;
}

void execute_pipe(char *left_cmd[], char *right_cmd[]) {

    if (left_cmd == NULL || left_cmd[0] == NULL) {
        display_error("Error: Left command is missing.\n", "");
        return;
    }
    if (right_cmd != NULL && (right_cmd[0] == NULL)) {
        display_error("Error: Right command is missing.\n", "");
        return;
    }

    int pipe_fd[2];
    pid_t pid1, pid2;

    if (pipe(pipe_fd) == -1) {
        perror("pipe");
        exit(1);
    }

    if ((pid1 = fork()) == 0) {

        dup2(pipe_fd[1], STDOUT_FILENO);
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        execvp(PATH, left_cmd);
        perror("execvp");
        exit(1);
    }

    if ((pid2 = fork()) == 0) {

        dup2(pipe_fd[0], STDIN_FILENO);
        close(pipe_fd[1]);
        close(pipe_fd[0]);
        execvp(PATH, right_cmd);
        perror("execvp");
        exit(1);
    }

    close(pipe_fd[0]);
    close(pipe_fd[1]);
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
}

ssize_t bn_kill(char **tokens) {
    if (check_length(tokens, 3) == -1){
        return -1;//too many args
    }
    for (int i = 0; i < process_count; i++) {
        char str[MAX_STR_LEN];
        sprintf(str, "%d", processes[i].pid);

        if (processes[i].status == 1 && strcmp(str, tokens[1]) == 0) {
            if (kill((pid_t)atoi(tokens[1]), atoi(tokens[2])) == 0) {
                printf("Process terminated successfully.\n");
            } else {
                return -1;//invalid signal
            }
        } else {
          return -1;//invalid pid
        }
    }
    return 0;
}

ssize_t bn_ps(char **tokens) {
    if (tokens[1] != NULL) {
      return -1;
    }
    for (int i = 0; i < process_count; i++) {
      if (processes[i].status == 1) {
          display_message(processes[i].outprint);
      }
    }
    return 0;
}

ssize_t bn_start_server(char **tokens) {
    if (check_length(tokens, 2) == -1){
        return -1;
    }
    setbuf(stdout, NULL);

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        perror("signal");
        exit(1);
    }

    struct client_sock *clients = NULL;

    struct listen_sock s;
    setup_server_socket(&s, atoi(tokens[1]));

    // Set up SIGINT handler
    struct sigaction sa_sigint;
    memset (&sa_sigint, 0, sizeof (sa_sigint));
    sa_sigint.sa_handler = sigint_handler;
    sa_sigint.sa_flags = 0;
    sigemptyset(&sa_sigint.sa_mask);
    sigaction(SIGINT, &sa_sigint, NULL);

    int exit_status = 0;

    int max_fd = s.sock_fd;

    fd_set all_fds, listen_fds;

    FD_ZERO(&all_fds);
    FD_SET(s.sock_fd, &all_fds);

    do {
        listen_fds = all_fds;
        int nready = select(max_fd + 1, &listen_fds, NULL, NULL, NULL);
        if (sigint_received) break;
        if (nready == -1) {
            if (errno == EINTR) continue;
            perror("server: select");
            exit_status = 1;
            break;
        }

        /*
         * If a new client is connecting, create new
         * client_sock struct and add to clients linked list.
         */
        if (FD_ISSET(s.sock_fd, &listen_fds)) {
            int client_fd = accept_connection(s.sock_fd, &clients);
            if (client_fd < 0) {
                printf("Failed to accept incoming connection.\n");
                continue;
            }
            if (client_fd > max_fd) {
                max_fd = client_fd;
            }
            FD_SET(client_fd, &all_fds);
            printf("Accepted connection\n");
        }

        if (sigint_received) break;

        /*
         * Accept incoming messages from clients,
         * and send to all other connected clients.
         */
        struct client_sock *curr = clients;
        while (curr) {
            if (!FD_ISSET(curr->sock_fd, &listen_fds)) {
                curr = curr->next;
                continue;
            }
            int client_closed = read_from_client(curr);

            // If error encountered when receiving data
            if (client_closed == -1) {
                client_closed = 1; // Disconnect the client
            }

            // If received at least one complete message
            // and client is newly connected: Get username
            if (client_closed == 0 && curr->username == NULL) {
                if (set_username(curr) == -1) {
                    printf("Error processing user name from client %d.\n", curr->sock_fd);
                    client_closed = 1; // Disconnect the client
                }
                else {
                    printf("Client %d user name is %s.\n", curr->sock_fd, curr->username);
                }
            }

            char *msg;
            // Loop through buffer to get complete message(s)
            while (client_closed == 0 && !get_message(&msg, curr->buf, &(curr->inbuf))) {
                printf("Echoing message from %s.\n", curr->username);
                char write_buf[BUF_SIZE];
                write_buf[0] = '\0';
                strncat(write_buf, curr->username, MAX_NAME);
                strncat(write_buf, " ", MAX_NAME);
                strncat(write_buf, msg, MAX_USER_MSG);
                free(msg);
                int data_len = strlen(write_buf);

                struct client_sock *dest_c = clients;
                while (dest_c) {
                    if (dest_c != curr) {
                        int ret = write_buf_to_client(dest_c, write_buf, data_len);
                        if (ret == 0) {
                            printf("Sent message from %s (%d) to %s (%d).\n",
                                curr->username, curr->sock_fd,
                                dest_c->username, dest_c->sock_fd);
                        }
                        else {
                            printf("Failed to send message to user %s (%d).\n", dest_c->username, dest_c->sock_fd);
                            if (ret == 2) {
                                printf("User %s (%d) disconnected.\n", dest_c->username, dest_c->sock_fd);
                                close(dest_c->sock_fd);
                                FD_CLR(dest_c->sock_fd, &all_fds);
                                assert(remove_client(&dest_c, &clients) == 0); // If this fails we have a bug
                                continue;
                            }
                        }
                    }
                    dest_c = dest_c->next;
                }
            }

            if (client_closed == 1) { // Client disconnected
                // Note: Never reduces max_fd when client disconnects
                FD_CLR(curr->sock_fd, &all_fds);
                close(curr->sock_fd);
                printf("Client %d disconnected\n", curr->sock_fd);
                assert(remove_client(&curr, &clients) == 0); // If this fails we have a bug
            }
            else {
                curr = curr->next;
            }
        }
    } while(!sigint_received);

    clean_exit(s, clients, exit_status);
    return 0;
}

ssize_t bn_end_server(char **tokens) {
    if (check_length(tokens, 1) == -1){
        return -1;
    }
    return 0;
}

ssize_t bn_send_message(char **tokens) {
    if (check_length(tokens, 4) == -1){
        return -1;
    }
    return 0;
}

ssize_t bn_start_client(char **tokens) {
    if (check_length(tokens, 3) == -1){
        return -1;
    }
    return 0;
}
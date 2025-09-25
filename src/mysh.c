#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

#include "builtins.h"
#include "io_helpers.h"
#include "variables.h"
#include "commands.h"

char **var_keys, **var_vals;
int *var_size;
Process processes[MAX_PROCESSES];
int process_count = 0;
int next_job_id = 1;

void sigint_handler(int signo) {
    (void)signo;

    display_message("\nmysh$ ");
    fflush(stdout);
}

void signal_handler(int signo) {
    printf("Caught signal %d (%s)\n", signo, strsignal(signo));

    if (signo == SIGINT) {
        display_message("\nmysh$ ");
        fflush(stdout);
    } else if (signo == SIGCHLD) {
        printf("Caught signal stupidahh)\n");
        int status;
        pid_t pid;

        while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
            for (int i = 0; i < process_count; i++) {
                if (processes[i].pid == pid) {
                    char str[MAX_STR_LEN];

                    display_message("\n[");

                    sprintf(str, "%d", processes[i].job_id);
                    display_message(str);
                    display_message("]+ Done ");

                    int j = 0;
                    while (processes[i].command[j] != NULL) {
                        display_message(processes[i].command[j]);
                        display_message(" ");  // Add space between command elements
                        j++;
                    }
                    display_message("\nmysh$ ");

                    processes[i].status = 0;
                    break;
                }
            }
        }
        fflush(stdout);
    }
}

void sigchild_handler(int signo) {
    (void)signo;
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < process_count; i++) {
            if (processes[i].pid == pid) {
                char str[MAX_STR_LEN];

                display_message("\n[");

                sprintf(str, "%d", processes[i].job_id);
                display_message(str);
                display_message("]+ Done ");

                int j = 0;
                while (processes[i].command[j] != NULL) {
                    display_message(processes[i].command[j]);
                    display_message(" ");  // Add space between command elements
                    j++;
                }
                display_message("\nmysh$ ");

                processes[i].status = 0;
                break;
            }
        }
    }
    fflush(stdout);
}

// You can remove __attribute__((unused)) once argc and argv are used.
int main(__attribute__((unused)) int argc, char* argv[]) {

    if (strcmp(argv[0], "./mysh") != 0) {
      run_builtins(argv);
      printf("Done: \n");
      return 1;
    }

    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    for (int signo = 1; signo < NSIG; signo++) {
        if (signo == SIGKILL || signo == SIGSTOP) {
            continue;
        }
        sigaction(signo, &sa, NULL);
    }

    initialize_variables(&var_keys, &var_vals, &var_size);

    char *prompt = "mysh$ "; // TODO Step 1, Uncomment this.

    char input_buf[MAX_STR_LEN + 1];
    input_buf[MAX_STR_LEN] = '\0';
    char *token_arr[MAX_STR_LEN] = {NULL};

    while (1) {
        // Prompt and input tokenization
        // Display the prompt via the display_message function.
	display_message(prompt);

        int ret = get_input(input_buf);
        size_t token_count = tokenize_input(input_buf, token_arr);

	// Clean exit
        // TODO: The next line has a subtle issue.
        if (ret == 0 || (ret != -1 && (token_count == 1 && strcmp("exit", token_arr[0]) == 0))) {
            break;
        }

        int background = 0;
        if (token_count > 0 && strcmp(token_arr[token_count - 1], "&") == 0) {
            token_arr[token_count - 1] = NULL;
            token_count--;

            background = 1;

            pid_t pid = fork();//if child, background by creating a procces in bg, else continue as planned

            if (pid < 0) {
                perror("Fork failed");
                return -1;
            }
            if (pid == 0) {
                run_builtins(token_arr);
                exit(1);
            } else {
                processes[process_count].job_id = next_job_id++;
                processes[process_count].pid = pid;

                for (int i = 0; i < MAX_STR_LEN && token_arr[i] != NULL; i++) {
                    processes[process_count].command[i] = strdup(token_arr[i]);
                }

                sprintf(processes[process_count].outprint, "%s %d\n",
                    processes[process_count].command[0],
                    processes[process_count].pid);
                processes[process_count].status = 1;

                char str[MAX_STR_LEN];

                display_message("[");

                sprintf(str, "%d", processes[process_count].job_id);
                display_message(str);
                display_message("] ");
                sprintf(str, "%d", processes[process_count].pid);
                display_message(str);
                display_message("\n");

                process_count++;
            }
        }

        // Command execution
        if (token_count >= 1 && background == 0) {

            bn_ptr builtin_fn = check_builtin(token_arr[0]);
            if (builtin_fn != NULL) {
                ssize_t err = builtin_fn(token_arr);
                if (err == - 1) {
                    display_error("ERROR: Builtin failed: ", token_arr[0]);
                }
            } else {
                pid_t pid = fork();

                if (pid < 0) {
                    display_error("ERROR: Fork failed: ", token_arr[0]);
                    exit(-1);
                } else if (pid == 0) {
                    if (execvp(token_arr[0], token_arr) == -1) {
                        display_error("ERROR: Unknown command: ", token_arr[0]);
                        exit(-1);
                    }
                } else {
                    int status;
                    waitpid(pid, &status, 0);
                }
            }
        }
    }
    for (int i = 0; i < process_count; i++) {
        for (int j = 0; j < MAX_STR_LEN && processes[i].command[j] != NULL; j++) {
            free(processes[i].command[j]);
        }
    }
    return 0;
}

void run_builtins(char **token_arr) {
    bn_ptr builtin_fn = check_builtin(token_arr[0]);
    if (builtin_fn != NULL) {
        ssize_t err = builtin_fn(token_arr);
        if (err == - 1) {
            display_error("ERROR: Builtin failed: ", token_arr[0]);
        }
    } else {
        pid_t pid = fork();

        if (pid < 0) {
            display_error("ERROR: Fork failed: ", token_arr[0]);
            exit(-1);
        } else if (pid == 0) {
            if (execvp(token_arr[0], token_arr) == -1) {
                display_error("ERROR: Unknown command: ", token_arr[0]);
                exit(-1);
            }
        } else {
            int status;
            waitpid(pid, &status, 0);
        }
    }
}
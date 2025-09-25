#ifndef VARIABLES_H
#define VARIABLES_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_STR_LEN 128

#define MAX_PROCESSES 100

typedef struct {
    int status;
    int job_id;
    pid_t pid;
    char outprint[MAX_STR_LEN];
    char *command[MAX_STR_LEN];
} Process;

void initialize_variables(char ***var_keys, char ***var_vals, int **var_size);
void assign_variables(char ***var_keys, char ***var_vals, int **var_size, const char *var_name, const char *new_val);
char *print_variables(char ***var_keys, char ***var_vals, int **var_size, const char *var_name);
void free_variables(char ***var_keys, char ***var_vals, int **var_size);

extern char **var_keys;
extern char **var_vals;
extern int *var_size;

extern Process processes[MAX_PROCESSES];
extern int process_count;
extern int next_job_id;

#endif

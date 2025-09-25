#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "builtins.h"
#include "io_helpers.h"
#include "variables.h"


void initialize_variables(char ***var_keys, char ***var_vals, int **var_size) {
    // Allocate memory for variables
    *var_keys = (char **)malloc(sizeof(char *));
    *var_vals = (char **)malloc(sizeof(char *));

    *var_size = (int *)malloc(sizeof(int));

    **var_size = 0;
}


void assign_variables(char ***var_keys, char ***var_vals, int **var_size, const char *var_name, const char *new_val) {
    char *processed_val = process_token((char *)new_val);
    for (int i = 0; i < **var_size; i++) {

        if (strcmp((*var_keys)[i], var_name) == 0) {

            free((*var_vals)[i]);
            (*var_vals)[i] = strdup(processed_val);
            free(processed_val);
            return;
        }
    }

    (**var_size)++;
    *var_keys = realloc(*var_keys, (**var_size) * sizeof(char *));
    *var_vals = realloc(*var_vals, (**var_size) * sizeof(char *));

    (*var_keys)[**var_size-1] = strdup(var_name);
    (*var_vals)[**var_size-1] = strdup(processed_val);
    free(processed_val);
}

char *print_variables(char ***var_keys, char ***var_vals, int **var_size, const char *var_print) {
    for (int i = 0; i < **var_size; i++) {
        if (strcmp((*var_keys)[i], var_print) == 0) {
            return (*var_vals)[i];
        }
    }
    return "";
}

void free_variables(char ***var_keys, char ***var_vals, int **var_size) {
    for (int i = 0; i < **var_size; i++) {
        free((*var_keys)[i]);
        free((*var_vals)[i]);
    }
    free(*var_keys);
    free(*var_vals);
    free(*var_size);
}

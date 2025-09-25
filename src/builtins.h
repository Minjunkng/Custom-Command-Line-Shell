#ifndef __BUILTINS_H__
#define __BUILTINS_H__

#include <unistd.h>


/* Type for builtin handling functions
 * Input: Array of tokens
 * Return: >=0 on success and -1 on error
 */
typedef ssize_t (*bn_ptr)(char **);
ssize_t bn_echo(char **tokens);
ssize_t bn_assign(char **tokens);
ssize_t assign_variable(char **tokens);
char *process_token(char *token);

ssize_t bn_ls(char **tokens);
ssize_t list_directory(const char *path, const char *filter, int recursive, int depth, int current_depth);

ssize_t bn_cd(char **tokens);
ssize_t bn_cat(char **tokens);
ssize_t bn_wc(char **tokens);
ssize_t check_length(char **tokens, int length);

int check_for_pipe(char **tokens);
void execute_pipe(char *left_cmd[], char *right_cmd[]);

ssize_t bn_kill(char **tokens);
ssize_t bn_ps(char **tokens);

ssize_t bn_start_server(char **tokens);
ssize_t bn_close_server(char **tokens);
ssize_t bn_send_message(char **tokens);
ssize_t bn_start_client(char **tokens);

/* Return: index of builtin or -1 if cmd doesn't match a builtin
 */
bn_ptr check_builtin(const char *cmd);


/* BUILTINS and BUILTINS_FN are parallel arrays of length BUILTINS_COUNT
 */
static const char * const BUILTINS[] = {"echo", "ls", "cd", "cat", "wc", "kill", "ps", "start-server",
                                       "close-server", "send", "start"};
static const bn_ptr BUILTINS_FN[] = {bn_echo, bn_ls, bn_cd, bn_cat, bn_wc , bn_kill, bn_ps ,
                                     bn_start_server, bn_close_server, bn_send_message, bn_start_client NULL};    // Extra null element for 'non-builtin'
static const ssize_t BUILTINS_COUNT = sizeof(BUILTINS) / sizeof(char *);

#endif

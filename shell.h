#ifndef SHELL_H
#define SHELL_H

#include <stdio.h> /* FILE* */

#define ARG_LONG(name, str_val, out, range_expr) do {             \
    long arg;                                                     \
    errno = 0;                                                    \
    arg = strtol(str_val, NULL, 0);                               \
    if ((errno == ERANGE && (arg == LONG_MAX || arg == LONG_MIN)) \
        || (errno != 0 && arg == 0)) {                            \
        fprintf (stderr, name": must be a digit\n");              \
        return 0;                                                 \
    }                                                             \
    if (!(range_expr)) {                                          \
        fprintf (stderr, name": must be in range '%s'\n", #range_expr);     \
        return 0;                                                 \
    }                                                             \
    out = arg;                                                    \
} while(0)

struct shell_config;
typedef int (*command_handler)(struct shell_config *sc, char *argv[], int argc);

struct commands_t {
    char *name;
    command_handler handler;
    char *help;
    char *usage;
};

struct shell_config {
    /* data need to be filled by user */
    char *progname;
    FILE *input;
    char *prompt;
    void *cookie;  /** point to custom data for commands */
    struct commands_t *commands;

    /* internal data */
    int   shell_quit;
    char *shell_input;
    char *history_file;
};

void rl_callback(char* line);
void shell_init(struct shell_config *sc);
void shell_deinit(struct shell_config *sc);
int  shell_handle(struct shell_config *sc);
void shell_show_help(struct shell_config *sc, char *name);

#endif

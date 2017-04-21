#ifndef SHELL_H
#define SHELL_H

#include <stdio.h> /* FILE* */

struct shell_config {
    /* data need to be filled by user */
    char *progname;
    FILE *input;
    char *prompt;
    void *cookie;  /** point to custom data for commands */

    /* internal data */
    int   shell_quit;
    char *shell_input;
    char *history_file;
};

void rl_callback(char* line);
void shell_init(struct shell_config *sc);
void shell_deinit(struct shell_config *sc);
int  shell_handle(struct shell_config *sc);

#endif

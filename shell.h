#ifndef SHELL_H
#define SHELL_H

#include <stdio.h> /* FILE* */

void rl_callback(char* line);
void shell_init(char *progname, FILE *input, char *prompt_);
void shell_deinit();
int  shell_handle();

#endif

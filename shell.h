#ifndef SHELL_H
#define SHELL_H

#include <stdio.h> /* FILE* */

void rl_callback(char* line);
// void shell_init();
void shell_init(char* progname);
void shell_init_from_file(char *progname, char *filePath);
void shell_deinit();
int  shell_handle();

#endif

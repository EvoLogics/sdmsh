#ifndef SHELL_HISTORY_H
#define SHELL_HISTORY_H

#include <shell.h>

void shell_history_init(struct shell_config *sc);
void shell_history_deinit(struct shell_config *sc);
void shell_add_history(struct shell_config *sc, char *cmdline);

void shell_show_history(int hist_num);
#endif


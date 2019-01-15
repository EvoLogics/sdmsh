#ifndef COMPAT_READLINE6_H
#define COMPAT_READLINE6_H

#ifdef COMPAT_READLINE6
#include <stdio.h>

/* Internal readline stuff */
extern int _rl_echoing_p;
extern char* _rl_term_cr;
extern int _rl_last_c_pos;
extern int _rl_last_v_pos;
extern int _rl_vis_botlin;

int _rl_output_character_function(int c);
int tputs(const char *str, int affcnt, int (*putc)(int));
void _rl_move_vert(int to);
void _rl_clear_to_eol(int count);

/* Compatibility readline stuff */
extern int history_offset;
int rl_tty_set_echoing(int u);
int rl_clear_visible_line();

#endif

#endif


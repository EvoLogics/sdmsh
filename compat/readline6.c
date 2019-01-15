/* compat.c -- backwards compatibility functions for readline6 */

#include <compat/readline6.h>
#include <readline/readline.h>

/* Set readline's idea of whether or not it is echoing output to the terminal,
   returning the old value. */
int
rl_tty_set_echoing (u)
     int u;
{
  int o;

  o = _rl_echoing_p;
  _rl_echoing_p = u;
  return o;
}

/* Clear all screen lines occupied by the current readline line buffer
   (visible line) */
int
rl_clear_visible_line ()
{
  int curr_line;

  /* Make sure we move to column 0 so we clear the entire line */
#if defined (__MSDOS__)
  putc ('\r', rl_outstream);
#else
  tputs (_rl_term_cr, 1, _rl_output_character_function);
#endif
  _rl_last_c_pos = 0;

  /* Move to the last screen line of the current visible line */
  _rl_move_vert (_rl_vis_botlin);

  /* And erase screen lines going up to line 0 (first visible line) */
  for (curr_line = _rl_last_v_pos; curr_line >= 0; curr_line--)
    {
      _rl_move_vert (curr_line);
      _rl_clear_to_eol (0);
    }

  return 0;
}


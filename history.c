#define _GNU_SOURCE
#include <stdio.h> /* asprintf() */
#include <stdlib.h>

#include <readline/readline.h>
#include <readline/history.h>

#include <history.h>

static int is_no_history_cmd(struct shell_config *sc, char *cmd_line);

void shell_history_init(struct shell_config *sc)
{
    char *homedir = getenv("HOME");

    using_history();

    history_write_timestamps = 1; /* ?? do not work? test it */
    history_comment_char = '#';

    if (!history_max_entries)
        history_max_entries = 1000;
    rl_read_init_file(NULL);

    if (homedir) {
        asprintf(&sc->history_file, "%s/.%s_history", homedir, sc->progname);
        read_history(sc->history_file);
    }
}

void shell_history_deinit(struct shell_config *sc)
{
    if (sc->flags & SF_SCRIPT_MODE)
        return;

    if (sc->history_file) {
        stifle_history(history_max_entries);
        write_history(sc->history_file);
        free(sc->history_file);
        sc->history_file = NULL;
    }
}

void shell_add_history(struct shell_config *sc, char *cmdline)
{
    int off;

    if (sc->flags & SF_SCRIPT_MODE)
        return;

    if(is_no_history_cmd(sc, cmdline))
        return;

    off = where_history();

    if (off > 0)
        off++;

    HIST_ENTRY *curr = history_get(off - 1);
    if (curr && !strcmp(curr->line, cmdline))
        return;

    add_history(cmdline);

    /*
     * usually add_history() placed in rl_cb_getline(), but if not
     * active history entry after rl_cb_getline() stay in prevision
     * entry. So next_history() must be called
     */
    next_history();
}

void shell_show_history(int hist_num)
{
    int hist_show_idx, i;
    HIST_ENTRY **hist_list;
    struct tm tm;
    char buf[0xff];
    char *timestamp = NULL;

    char *timefmt = getenv("HISTTIMEFORMAT");

    if (hist_num <= 0)
        hist_num = 10;

    hist_list = history_list();
    if (!hist_list)
        return;

    memset(&tm, 0, sizeof(struct tm));

    hist_show_idx = history_offset - hist_num > 0 ? history_offset - hist_num : 0;

    for (i = 0; hist_list[i]; i++)
        if (hist_show_idx <= i) {
            if (timefmt) {
                strptime(hist_list[i]->timestamp + 1, "%s", &tm);
                strftime(buf, sizeof(buf), timefmt, &tm);
                timestamp = buf;
            }
            printf("% 5d %s%s\n", i + history_base, timestamp ? timestamp : " ", hist_list[i]->line);
        }

}

static int is_no_history_cmd(struct shell_config *sc, char *cmd_line)
{
    struct commands_t *cmd;
    int rc = 0;
    char *c = strdup(cmd_line);
    strtok(c, " ");
    for (cmd = sc->commands; cmd->name != NULL; cmd++)
        if (!strcmp("history", c)) {
            rc = 1;
            break;
        }
    free(c);
    return rc;
}

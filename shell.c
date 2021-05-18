/* for asprintf() */
#define _GNU_SOURCE

#include <assert.h>
#include <err.h>
#include <stdarg.h>
#include <stdio.h>  /* FILE* */
#include <stdlib.h>
#include <string.h> /* strdup() */
#include <sys/queue.h>
#include <wordexp.h>

#include <compat/readline6.h>
#include <readline/readline.h>

#include <shell.h>
#include <shell_completion.h>
#include <shell_history.h>
#include <shell_help.h>
#include <utils.h>

int shell_run_cmd(struct shell_config *sc, char *shell_input);

/* needed for rb_cb_getline() and such callback function */
static struct shell_config *shell_config;

void rl_cb_getline(char* line)
{
    shell_config->shell_input = line;
    if (line == NULL) {
        shell_config->shell_quit = 1;
        return;
    }
}

int is_interactive_mode(struct shell_config *sc)
{
    return !(sc->flags & SF_SCRIPT_MODE);
}

static void handle_signals(int signo) {
    switch (signo) {
        case SIGINT:
            if (is_interactive_mode(shell_config)) {
                if (rl_line_buffer) {
                    rl_replace_line("", 0);
                    rl_redisplay();
                }
            } else {
                shell_config->shell_quit = 1;
            }
            shell_config->sig_cancel++;
            break;

        case SIGPIPE:
            break;

        default:
            break;
    }

    if (shell_config->signal_event_hook)
        shell_config->signal_event_hook(signo);
}

void shell_init(struct shell_config *sc)
{
    struct sigaction act;
    shell_config = sc;

    sc->shell_input = NULL;
    sc->history_file = NULL;

    if (STAILQ_EMPTY(&sc->inputs_list))
        shell_input_add(sc, SHELL_INPUT_TYPE_STDIO);

    if (!is_interactive_mode(sc)) {
        sc->prompt = strdup("");
    } else {
        if (sc->prompt == NULL)
            sc->prompt = strdup("> ");

        shell_history_init(sc);
        shell_completion_init(sc);

        /* Allow conditional parsing of the ~/.inputrc file. */
        rl_readline_name = sc->progname;
    }

    if (sigaction(SIGINT, NULL, &act))
        logger(WARN_LOG, "Failed set signal handler\n");

    /* Clear SA_RESTART, we need interrupt named pipes */
    act.sa_flags &= ~SA_RESTART;
    act.sa_handler = handle_signals;
    if (sigaction(SIGINT, &act, NULL))
        logger(WARN_LOG, "Failed set SIGINT signal handler\n");
    if (sigaction(SIGPIPE, &act, NULL))
        logger(WARN_LOG, "Failed set SIGPIPE signal handler\n");


    rl_set_signals();

    shell_input_init_current(sc);
}

void shell_deinit(struct shell_config *sc)
{
    if (is_interactive_mode(sc)) {
        rl_clear_visible_line();
        shell_history_deinit(sc);
    }

    rl_callback_handler_remove();
    rl_clear_signals();
    signal(SIGINT, SIG_IGN);

    free(sc->prompt);
}

int  shell_handle(struct shell_config *sc)
{
    int rc = 0;
    char *cmd;
    if (sc->shell_quit) {
        struct shell_input *si = shell_input_next(sc);

        if (si == NULL) {
            if (sc->shell_input) {
                free(sc->shell_input);
                sc->shell_input = NULL;
            }
            return SHELL_EOF;
        }

        /* Hack to show prompt after `source` */
        if (is_interactive_mode(sc)) {
            rl_stuff_char('\n');
            rl_callback_read_char();
        }
    }

    if (!sc->shell_input)
        return 0;

    cmd = strchopspaces(sc->shell_input);
    if (!cmd || cmd[0] == 0 || cmd[0] == '#')
        goto shell_handle_free;

    sc->sig_cancel = 0;
    shell_add_history(shell_config, cmd);

    /* Handle commands separated by ';' */
    if (strchr(cmd, ';')) {
        if (is_interactive_mode(sc))
            rl_clear_visible_line();
        rc = shell_input_add(sc, SHELL_INPUT_PUT_HEAD|SHELL_INPUT_TYPE_STRING, cmd);
        if (rc == -1) {
            fprintf(stderr, "Error open file /dev/zero\n");
            goto shell_handle_free;
        } else if (rc == -2) {
            fprintf(stderr, "Too many inputs. Maximum %d", SHELL_MAX_INPUT);
            rc = -1;
            goto shell_handle_free;
        }
        shell_input_init_current(sc);
        goto shell_handle_free;
    }
    rc = shell_run_cmd(sc, cmd);

    /*
     * If command report error, and first input is interactive
     * we need remove all inputs but first one.
     * For example `source` script call another `source` script
     * and in this script command with error, we need return
     * to first interactive shell.
     */
    if (rc < 0) {
        struct shell_input *sil = STAILQ_LAST(&sc->inputs_list, shell_input, next_input);
        if (sil->flags & SHELL_INPUT_INTERACTIVE) {
            while (sil != STAILQ_FIRST(&sc->inputs_list)) {
                STAILQ_REMOVE_HEAD(&sc->inputs_list, next_input);
            }
            shell_input_init_current(sc);
        }
    }

shell_handle_free:
    free(sc->shell_input);
    sc->shell_input = NULL;

    return rc;
}

int shell_make_argv(char *cmd_line, char ***argv, int *argc)
{
    wordexp_t result;

    switch (wordexp(cmd_line, &result, 0))
    {
        case 0:
            break;
        case WRDE_NOSPACE:
            wordfree (&result);
            /* FALLTHROUGH */
        default:
            return -1;
    }

    *argc = result.we_wordc;
    *argv = result.we_wordv;

    return 0;
}

void shell_free_argv(char **argv, int argc)
{
    int i;
    for (i = 0; i < argc; i++)
        free(argv[i]);
    free(argv);
}

int shell_run_cmd(struct shell_config *sc, char *shell_input)
{
    struct commands_t *cmd;
    char **argv;
    int argc;
    int rc;
    char *cmd_line;

    if (!shell_input || *shell_input == 0)
        return -1;

    cmd_line = strdup(shell_input);

    if (shell_make_argv(cmd_line, &argv, &argc) == -1) {
        fprintf(stderr, "\rCommand syntax error: \"%s\"\n", cmd_line);
        free(cmd_line);
        return -1;
    }

    logger(DEBUG_LOG, "call: %s\n", cmd_line);

    for (cmd = sc->commands; cmd->name != NULL; cmd++) {
        if (!strcmp(cmd->name, argv[0])) {
            if (is_interactive_mode(sc))
                rl_clear_visible_line();

            rc = cmd->handler(sc, argv, argc);

            shell_forced_update_display(sc);
            break;
        }
    }

    if (cmd->name == NULL) {
        fprintf(stderr, "\rUnknown command: %s\n", argv[0]);
        shell_forced_update_display(sc);
    }

    free(cmd_line);
    shell_free_argv(argv, argc);
    return rc;
}

void shell_forced_update_display(struct shell_config *sc)
{
    if (is_interactive_mode(sc))
        rl_forced_update_display();
}

int rl_hook_argv_getch(FILE *in)
{
    static char prev_ch = 0;
    char ch;
    struct shell_input *si;
    in = in;

    si = STAILQ_FIRST(&shell_config->inputs_list);
    if (*si->pos != 0) {
        ch = *si->pos++;
    } else {
        /* This is workaround bug when running more then
         * two commands in interactive shell separated by ';'.
         *
         * Looks like bug in readline(?).
         * From documentation:
         * If `readline` encounters an EOF while reading
         * the line, and the * line is empty at that point,
         * then (char *)NULL * is returned.  Otherwise, the
         * line is ended just as >>if a newline had been typed<<.
         *
         * SHELL_INPUT_TYPE_STRING works in argv, but did't work
         * from interactive shell. Why?
         *
         */

        if (prev_ch == '\n')
            return EOF;
        ch = '\n';
    }
    prev_ch = ch;

    if (ch == 0) {
        ch = '\n';
    } else if (ch == ';')
        ch = '\n';

    return ch;
}

void shell_update_prompt(struct shell_config *sc, char *fmt, ...)
{
    va_list ap;
    char *line_buf = NULL;

    if (rl_line_buffer)
        line_buf = strdup(rl_line_buffer);

    if (sc->prompt)
        free(sc->prompt);

    va_start(ap, fmt);
    vasprintf(&sc->prompt, fmt, ap);
    va_end(ap);

    rl_callback_handler_install(sc->prompt, rl_cb_getline);

    if (line_buf) {
        rl_insert_text(line_buf);
        free(line_buf);
    }

    rl_refresh_line(0, 0);
}

void shell_input_init(struct shell_config *sc)
{
    STAILQ_INIT(&sc->inputs_list);
    sc->inputs_count = 0;
}

int shell_input_add(struct shell_config *sc, int flags, ...)
{
    va_list ap;
    struct shell_input *si;
    int type = flags & SHELL_INPUT_MASK_TYPE;

    if (sc->inputs_count >= SHELL_MAX_INPUT)
        return -2;

    si = malloc(sizeof(struct shell_input));
    if (!si)
        return -2;

    if (flags & SHELL_INPUT_PUT_HEAD)
        STAILQ_INSERT_HEAD(&sc->inputs_list, si, next_input);
    else
        STAILQ_INSERT_TAIL(&sc->inputs_list, si, next_input);

    si->flags = flags;

    switch (type) {
        case SHELL_INPUT_TYPE_STDIO:
            si->input = stdin;
            si->source_name = "stdin";
            logger(DEBUG_LOG, "Use stdin.\n");
            break;

        case SHELL_INPUT_TYPE_FILE:
            va_start(ap, flags);
            si->script_file = va_arg(ap, char *);
            va_end(ap);

            if ((si->input = fopen(si->script_file, "r")) == NULL)
                return -1;
            logger(DEBUG_LOG, "Open script file \"%s\".\n", si->script_file);

            break;

        case SHELL_INPUT_TYPE_STRING:
            va_start(ap, flags);
            si->script_string = va_arg(ap, char *);
            si->pos = si->script_string;
            va_end(ap);

            /* just dummy for select() to make it always ready to read */
            if ((si->input = fopen("/dev/zero", "r")) == NULL)
                return -1;
            logger(DEBUG_LOG, "Add input of commands from string \"%s\".\n", si->script_string);

            break;

        default:
            assert(!"shell_add_input: unknown input type\n");
    }

    sc->inputs_count++;
    return 0;
}

void shell_input_init_current(struct shell_config *sc)
{
    struct shell_input *si = STAILQ_FIRST(&sc->inputs_list);

    if (!si)
        return;

    switch (si->flags & SHELL_INPUT_MASK_TYPE) {
        case SHELL_INPUT_TYPE_STDIO:
        case SHELL_INPUT_TYPE_FILE:
            if (si->input && isatty(fileno(si->input)))
                si->flags |= SHELL_INPUT_INTERACTIVE;
            else
                si->flags &= ~SHELL_INPUT_INTERACTIVE;

            rl_getc_function = rl_getc;
            break;

        case SHELL_INPUT_TYPE_STRING:
            rl_getc_function = rl_hook_argv_getch;
            break;

        default:
            ;
    }

    if (si->flags & SHELL_INPUT_INTERACTIVE) {
        sc->flags &= ~SF_SCRIPT_MODE;
        rl_outstream = stdout;
        si->output = NULL;
    } else {
        static FILE *dev_null = NULL;
        if (!dev_null)
            dev_null = fopen("/dev/null", "w");;
        sc->flags |= SF_SCRIPT_MODE;
        si->output = rl_outstream = dev_null;
        rl_tty_set_echoing(0);
    }

    rl_instream = sc->input = si->input;
    sc->shell_quit = 0;
    sc->sig_cancel = 0;
}

struct shell_input* shell_input_next(struct shell_config *sc)
{
    struct shell_input *si = STAILQ_FIRST(&sc->inputs_list);

    /* closing stdin will cause terminal corrupion after sdmsh quit */
    if (si->input && ((si->flags & SHELL_INPUT_MASK_TYPE ) != SHELL_INPUT_TYPE_STDIO))
        fclose(si->input);

    STAILQ_REMOVE_HEAD(&sc->inputs_list, next_input);

    if (STAILQ_EMPTY(&sc->inputs_list)) {
        if (si->output)
            fclose(si->output);
        free(si);
        return NULL;
    }
    free(si);

    si = STAILQ_FIRST(&sc->inputs_list);

    sc->inputs_count--;
    logger (DEBUG_LOG, "Init next source \"%s\"\n", si->source_name);

    shell_input_init_current(sc);

    return si;
}

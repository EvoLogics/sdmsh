#include <stdlib.h>
#include <stdarg.h>
#include <err.h>

#include <readline/readline.h>

#include <history.h>
#include <shell.h>
#include <utils.h>

int shell_run_cmd(struct shell_config *sc);

/* needed for rb_cb_getline() and rl_hook_argv_getch() */
static struct shell_config *shell_config;

void rl_cb_getline(char* line)
{
    shell_config->shell_input = line;
    if (line == NULL) {
        shell_config->shell_quit = 1;
        return;
    }
}

char* shell_rl_driver_completion(const char *text, int index)
{
    int cnt = 0;
    struct driver_t *drv;


    for (drv = shell_config->drivers; drv->name != NULL; drv++)
        if (*text == 0 || strstart(drv->name, text))
            if(cnt++ == index)
                return strdup(drv->name);

    for (drv = shell_config->drivers; drv->name != NULL; drv++)
        if (drv->flags & SF_DRIVER_FILENAME && strstart((char *)text, drv->name)) {
            char *fn;
            char *fn_with_prefix;

            rl_filename_completion_desired = 1;
            fn = rl_filename_completion_function(text + strlen(drv->name), index - cnt);
            if (!fn)
                return NULL;

            fn_with_prefix = malloc(strlen(fn) + strlen(drv->name) + 1 + 1);
            sprintf(fn_with_prefix, "%s%s", drv->name, fn);
            free(fn);
            return fn_with_prefix;
        }

    return rl_filename_completion_function(text, index - cnt);
}

char* shell_rl_driver_gen(const char *text, int state)
{
    static int list_index;
    char *name;

    if (!state)
        list_index = 0;

    if ((name = shell_rl_driver_completion(text, list_index)) != NULL) {
        list_index++;
        return name;
    }

    return NULL;
}


char* shell_rl_command_completion(const char *text, int index)
{
    char *name = NULL;
    int cnt = 0;
    struct commands_t *cmd;

    for (cmd = shell_config->commands; cmd->name != NULL; cmd++)
        if (*text == 0 || strstart(cmd->name, text))
            if(cnt++ == index) {
                name = cmd->name;
                break;
            }

    return name;
}

char* shell_rl_cmd_gen(const char *text, int state)
{
    static int list_index;
    char *name;

    if (!state)
        list_index = 0;

    if ((name = shell_rl_command_completion(text, list_index)) != NULL) {
        list_index++;
        return strdup(name);
    }

    return NULL;
}

char** shell_cb_completion(const char *text, int start, int end)
{
    char **matches = NULL;
    end = end;

    if (start == 0)
        matches = rl_completion_matches(text, shell_rl_cmd_gen);
    else if (!strncmp(rl_line_buffer, "help ", 5)) {
        rl_completion_suppress_append = 0;
        matches = rl_completion_matches(text, shell_rl_cmd_gen);
    } else if ((!strncmp(rl_line_buffer, "ref ", 4) && (start == 4 || start != end))
            || (!strncmp(rl_line_buffer, "tx ",  3) && (start == 3 || start != end))) {
        rl_completion_suppress_append = 1;
        matches = rl_completion_matches(text, shell_rl_driver_gen);
    }

    return matches;
}

char* shell_rl_hook_dummy(const char *text, int state)
{
    text = text; state = state;
    return NULL;
}

void shell_init(struct shell_config *sc)
{
    shell_config = sc;

    sc->shell_quit = 0;
    sc->shell_input = NULL;
    sc->history_file = NULL;

    if (sc->input != stdin || !isatty(STDIN_FILENO)) {
        sc->prompt = strdup("");
        rl_instream = sc->input;
        rl_outstream = sc->input;
    } else {
        if (sc->prompt == NULL)
            sc->prompt = strdup("> ");
        rl_instream = stdin;
        rl_outstream = stdout;

        shell_history_init(sc);

        /* Allow conditional parsing of the ~/.inputrc file. */
        rl_readline_name = sc->progname;
        rl_attempted_completion_function = shell_cb_completion;
        rl_completion_entry_function = shell_rl_hook_dummy;
    }
    rl_callback_handler_install(sc->prompt, (rl_vcpfunc_t*) &rl_cb_getline);
}

void shell_deinit(struct shell_config *sc)
{
    shell_history_deinit(sc);

    rl_callback_handler_remove();
    free(sc->prompt);
}

int  shell_handle(struct shell_config *sc)
{
    int rc = 0;
    char *p;
    if (sc->shell_quit)
        return SHELL_EOF;

    if (!sc->shell_input)
        return 0;

    p = strchopspaces(sc->shell_input);
    if (!p || p[0] == 0 || p[0] == '#')
        return 0;

    rc = shell_run_cmd(sc);

    shell_add_history(shell_config, p);

    free(sc->shell_input);
    sc->shell_input = NULL;

    return rc;
}

void shell_make_argv(char *cmd_line, char ***argv, int *argc)
{
    char *p;
    *argc = 0;
    *argv = calloc(1, sizeof(char *));

    for (p = strtok(cmd_line, " "); p; p = strtok(NULL, " ")) {
        (*argv)[(*argc)++] = p;
        *argv = realloc(*argv, (*argc + 1) * sizeof(char *));
    }

    (*argv)[*argc] = NULL;
}

int shell_run_cmd(struct shell_config *sc)
{
    struct commands_t *cmd;
    char **argv;
    int argc;
    int rc;
    char *cmd_line;

    if (!sc->shell_input || *sc->shell_input == 0)
        return -1;

    cmd_line = strdup(sc->shell_input);

    logger(DEBUG_LOG, "call: %s\n", cmd_line);
    shell_make_argv(cmd_line, &argv, &argc);
    if (argv == NULL || *argv == NULL) {
        free(cmd_line);
        return -1;
    }

    for (cmd = sc->commands; cmd->name != NULL; cmd++) {
        if (!strcmp(cmd->name, argv[0])) {
            printf ("\r");
            rc = cmd->handler(sc, argv, argc);
            rl_forced_update_display();
            break;
        }
    }

    if (cmd->name == NULL) {
        fprintf(stderr, "\rUnknown command: %s\n", argv[0]);
        rl_forced_update_display();
    }

    free(cmd_line);
    free(argv);
    return rc;
}

void shell_show_help_drivers(struct shell_config *sc, char *shift)
{
    struct driver_t *drv;

    printf("%s<driver> is:\n", shift);
    for (drv = sc->drivers; drv->name != NULL; drv++)
         printf("\t%s%s. %s\n", shift, drv->usage, drv->help);
}

void shell_show_help(struct shell_config *sc, char *name)
{
    struct commands_t *cmd;
    for (cmd = sc->commands; cmd->name != NULL; cmd++) {
        if (!name || !strcmp(cmd->name, name)) {
            printf ("%-10s-\t%s\n", cmd->name, cmd->help);
            printf ("%-10s \tUsage: %s\n", " ", cmd->usage ? cmd->usage : cmd->name);
            if (name) {
                if (cmd->flags & SF_USE_DRIVER)
                    shell_show_help_drivers(sc, "\t\t");
                break;
            }
        }
    }

    if (!name)
        shell_show_help_drivers(sc, "");

    if (name && cmd->name == NULL) {
        fprintf(stderr, "Unknown topic: %s\n", name);
    }
}

int rl_hook_argv_getch(FILE *in)
{
    int ch;
    in = in;

    if (shell_config->argv_input.optind == shell_config->argv_input.argc)
        return EOF;

    ch = shell_config->argv_input.argv[shell_config->argv_input.optind][shell_config->argv_input.pos++];

    if (ch == 0) {
        ch = '\n';
        shell_config->argv_input.optind++;
        shell_config->argv_input.pos = 0;
    } else if (ch == ';')
        ch = '\n';

    return ch;
}

void shell_init_input_argv(struct shell_config *sc, int argc, char **argv)
{
    sc->argv_input.argc = argc;
    sc->argv_input.argv = argv;
    sc->argv_input.optind  = 0;
    sc->argv_input.pos = 0;
    rl_getc_function = rl_hook_argv_getch;
}

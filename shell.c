#define _GNU_SOURCE
#include <stdio.h> /* asprintf() */

#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>

#include <shell.h>
#include <utils.h>

int shell_run_cmd(struct shell_config *sc);

/* needed for rb_cb_getline() */
static struct shell_config *shell_config;

void rl_cb_getline(char* line)
{
    shell_config->shell_input = line;
    if (line == NULL) {
        shell_config->shell_quit = 1;
        return;
    }

    if(strlen(line) > 0)
        add_history(line);
}

char* shell_rl_find_completion(const char *text, int index)
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

    if ((name = shell_rl_find_completion(text, list_index)) != NULL) {
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
        rl_completion_suppress_append = 1;
        matches = rl_completion_matches(text, shell_rl_cmd_gen);
    } else if ((!strncmp(rl_line_buffer, "ref ", 4) && (start == 4 || start != end))
            || (!strncmp(rl_line_buffer, "tx ",  3) && (start == 3 || start != end))) {
        matches = rl_completion_matches(text, rl_filename_completion_function);
        rl_completion_suppress_append = 1;
        rl_filename_completion_desired = 1;
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
    char *homedir = getenv("HOME");

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

        if (homedir) {
            asprintf(&sc->history_file, "%s/.%s_history", homedir, sc->progname);
            read_history(sc->history_file);
        }

        /* Allow conditional parsing of the ~/.inputrc file. */
        rl_readline_name = sc->progname;
        rl_attempted_completion_function = shell_cb_completion;
        rl_completion_entry_function = shell_rl_hook_dummy;
    }
    rl_callback_handler_install(sc->prompt, (rl_vcpfunc_t*) &rl_cb_getline);
}

void shell_deinit(struct shell_config *sc)
{
    if (sc->history_file) {
        write_history(sc->history_file);
        free(sc->history_file);
        sc->history_file = NULL;
    }

    rl_callback_handler_remove();
    free(sc->prompt);
}

int  shell_handle(struct shell_config *sc)
{
    if (sc->shell_quit)
        return 0;

    if (sc->shell_input != NULL) {
        shell_run_cmd(sc);

        free(sc->shell_input);
        sc->shell_input = NULL;
    }
    return 1;
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

    if (*sc->shell_input == 0) {
        free(sc->shell_input);
        sc->shell_input = NULL;
        return 1;
    }

    shell_make_argv(sc->shell_input, &argv, &argc);
    if (argv == NULL || *argv == NULL) {
        free(sc->shell_input);
        sc->shell_input = NULL;
        return 1;
    }

    for (cmd = sc->commands; cmd->name != NULL; cmd++) {
        if (!strcmp(cmd->name, argv[0])) {
            printf ("\r");
            cmd->handler(sc, argv, argc);
            rl_forced_update_display();
            break;
        }
    }

    if (cmd->name == NULL) {
        fprintf(stderr, "\rUnknown command: %s\n", argv[0]);
        rl_forced_update_display();
    }

    free(argv);
    free(sc->shell_input);
    sc->shell_input = NULL;

    return 0;
}

void shell_show_help(struct shell_config *sc, char *name)
{
    struct commands_t *cmd;
    for (cmd = sc->commands; cmd->name != NULL; cmd++) {
        if (!name || !strcmp(cmd->name, name)) {
            printf ("%-10s-\t%s\n", cmd->name, cmd->help);
            printf ("%-10s \tUsage: %s\n", " ", cmd->usage ? cmd->usage : cmd->name);
            if (name)
                break;
        }
    }
    if (name && cmd->name == NULL) {
        fprintf(stderr, "Unknown topic: %s\n", name);
    }
}

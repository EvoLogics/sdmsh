#define _GNU_SOURCE
#include <stdio.h> /* asprintf() */

#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>

#include <shell.h>
#include <commands.h>
#include <logger.h>

char *prompt;
int   shell_quit = 0;
char *shell_input = NULL;
char *history_file = NULL;

int shell_run_cmd();

void rl_cb_getline(char* line)
{
    shell_input = line;
    if (line == NULL) {
        shell_quit = 1;
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

    for (cmd = commands; cmd->name != NULL; cmd++)
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

void shell_init(char *progname, FILE *input, char *prompt_)
{
    char *homedir = getenv("HOME");

    if (input != stdin || !isatty(STDIN_FILENO)) {
        prompt = strdup("");
        rl_instream = input;
        rl_outstream = input;
    } else {
        prompt = strdup(prompt_);
        rl_instream = stdin;
        rl_outstream = stdout;

        if (homedir) {
            asprintf(&history_file, "%s/.%s_history", homedir, progname);
            read_history(history_file);
        }

        /* Allow conditional parsing of the ~/.inputrc file. */
        rl_readline_name = progname;
        rl_attempted_completion_function = shell_cb_completion;
        rl_completion_entry_function = shell_rl_hook_dummy;
    }
    rl_callback_handler_install(prompt, (rl_vcpfunc_t*) &rl_cb_getline);
}

void shell_deinit()
{
    if (history_file) {
        write_history(history_file);
        free(history_file);
        history_file = NULL;
    }

    rl_callback_handler_remove();
    free(prompt);
}

int shell_handle()
{
    if (shell_quit)
        return 0;

    if (shell_input != NULL) {
        shell_run_cmd();

        free(shell_input);
        shell_input = NULL;
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

int shell_run_cmd()
{
    struct commands_t *cmd;
    char **argv;
    int argc;

    if (*shell_input == 0) {
        free(shell_input);
        shell_input = NULL;
        return 1;
    }

    shell_make_argv(shell_input, &argv, &argc);
    if (argv == NULL || *argv == NULL) {
        free(shell_input);
        shell_input = NULL;
        return 1;
    }

    for (cmd = commands; cmd->name != NULL; cmd++) {
        if (!strcmp(cmd->name, argv[0])) {
            printf ("\r");
            cmd->handler(argv, argc);
            rl_forced_update_display();
            break;
        }
    }

    if (cmd->name == NULL) {
        fprintf(stderr, "\rUnknown command: %s\n", argv[0]);
        rl_forced_update_display();
    }

    free(argv);
    free(shell_input);
    shell_input = NULL;

    return 0;
}

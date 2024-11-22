#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <compat/readline6.h>
#include <readline/readline.h>

#include <shell_completion.h>
#include <utils.h>

typedef int shell_filter_func_t(char *name);

/* needed for callback function */
static struct shell_config *shell_config;

static char* shell_rl_driver_completion(const char *text, int index)
{
    int cnt = 0;
    struct driver_t *drv;


    for (drv = shell_config->drivers; drv->name != NULL; drv++)
        if (*text == 0 || strstart(drv->name, text))
            if(cnt++ == index)
                return strdup(drv->name);

    for (drv = shell_config->drivers; drv->name != NULL; drv++)
        if (drv->flags & SCF_DRIVER_FILENAME && strstart((char *)text, drv->name)) {
            char *fn;
            char *fn_with_prefix;
	    size_t len_fwp;

            rl_filename_completion_desired = 1;
            fn = rl_filename_completion_function(text + strlen(drv->name), index - cnt);
            if (!fn)
                return NULL;

	    len_fwp = strlen (fn) + strlen (drv->name) + 1 + 1;
            fn_with_prefix = malloc(len_fwp);
            snprintf(fn_with_prefix, len_fwp, "%s%s", drv->name, fn);
            free(fn);
            return fn_with_prefix;
        }

    return rl_filename_completion_function(text, index - cnt);
}

static char* shell_rl_driver_gen(const char *text, int state)
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


static char* shell_rl_command_completion(const char *text, int index)
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

static char* shell_rl_cmd_gen(const char *text, int state)
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

static int is_driver_completion_command(struct shell_config *sc, char *input)
{
    struct commands_t *cmd;

    for (cmd = sc->commands; cmd->name != NULL; cmd++)
        if (cmd->flags & SCF_USE_DRIVER) {
            unsigned int clen = strlen(cmd->name);
            if (clen + 1 <= strlen(input) && input[clen] == ' ')
                if (strstart(input, cmd->name))
                        return 1;
        }

    return 0;
}

static int shell_completion_filter(char **matches, shell_filter_func_t *filter_func)
{
    char **new_matches;
    int cnt = 0;
    int i, nidx;

    if (matches[1] == NULL)
        return 0;

    for (i = 1; matches[i]; i++)
        cnt++;

    new_matches = malloc(sizeof(char *) * (cnt + 1));
    new_matches[0] = matches[0];

    for (i = nidx = 1; matches[i]; i++) {
        if (filter_func(matches[i]))
            new_matches[nidx++] = matches[i];
        else
            free(matches[i]);
    }

    new_matches[nidx - 1] = NULL;
    for (i = 1; new_matches[i]; i++)
        matches[i] = new_matches[i];
    matches[nidx] = NULL;

    free(new_matches);

    return 0;
}

static int shell_filter_scripts_name(char *name)
{
    struct stat sb;
    char *p = strrchr(name, '.');

    if (p && !strcmp(p, ".sdmsh"))
        return 1;
    
    if (!stat(name, &sb))
        if (S_ISDIR(sb.st_mode))
                return 1;

    return 0;
}

static int shell_filter_scripts_names(char **names)
{
    return shell_completion_filter(names, shell_filter_scripts_name);
}

static char** shell_cb_completion(const char *text, int start, int end)
{
    char **matches = NULL;
    (void)end;

    rl_ignore_some_completions_function = NULL;

    if (start == 0)
        matches = rl_completion_matches(text, shell_rl_cmd_gen);
    else if (!strncmp(rl_line_buffer, "help ", 5)) {
        rl_completion_suppress_append = 0;
        matches = rl_completion_matches(text, shell_rl_cmd_gen);
    } else if (is_driver_completion_command(shell_config, rl_line_buffer)) {
        rl_completion_suppress_append = 1;
        rl_sort_completion_matches = 0;
        matches = rl_completion_matches(text, shell_rl_driver_gen);
    } else if ((!strncmp(rl_line_buffer, "source ", 7) && (start == 7 || start != end))) {
        rl_ignore_some_completions_function = shell_filter_scripts_names;
        rl_completion_mark_symlink_dirs = 1;
        matches = rl_completion_matches(text, rl_filename_completion_function);
    }

    return matches;
}

static char* shell_rl_hook_dummy(const char *text, int state)
{
    (void)text;
    (void)state;
    return NULL;
}

void shell_completion_init(struct shell_config *sc)
{
    shell_config = sc;
    rl_attempted_completion_function = shell_cb_completion;
    rl_completion_entry_function = shell_rl_hook_dummy;
}

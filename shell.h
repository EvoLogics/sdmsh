#ifndef SHELL_H
#define SHELL_H

/* for sighandler_t */
#ifndef __USE_GNU
#define __USE_GNU
#endif
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h> /* FILE* */
#include <signal.h>

#include <utils.h>

#define SHELL_EOF -256

#define ARGS_RANGE(range_expr) do {                            \
    if (!(range_expr)) {                                       \
        fprintf(stderr                                         \
                ,"%s: argument number must be in range '%s'\n" \
                , argv[0], #range_expr);                       \
        return -1;                                             \
    }                                                          \
} while(0)

#define SF_SCRIPT_MODE      (1 << 0)

#define SCF_NONE            0
#define SCF_USE_DRIVER      (1 << 0)
#define SCF_NO_HISTORY      (1 << 1)
#define SCF_DRIVER_FILENAME (1 << 2)
#define SCF_DRIVER_NET      (1 << 3)
#define SCF_DRIVER_SH_LINE  (1 << 4)

struct shell_config;
typedef int (*command_handler)(struct shell_config *sc, char *argv[], int argc);

struct commands_t {
    char *name;
    command_handler handler;
    unsigned long flags;
    char *usage;
    char *help;
};

struct driver_t {
    char *name;
    unsigned long flags;
    char *usage;
    char *help;
};

#define SHELL_INPUT_PUT_HEAD    (1 << 6)
#define SHELL_INPUT_INTERACTIVE (1 << 7)
#define SHELL_INPUT_MASK_TYPE   0x0f

enum {
    SHELL_INPUT_TYPE_STDIO = 1,
    SHELL_INPUT_TYPE_FILE  = 2,
    SHELL_INPUT_TYPE_STRING  = 3
};

struct shell_input;
struct shell_input {
    STAILQ_ENTRY(shell_input) next_input;

    int flags;
    FILE *input;
    FILE *output;
    union {
        char *source_name;
        char *script_file;
        struct {
            char *script_string;
            char *pos; /* current char in argument */
        };
    };
};

#define SHELL_MAX_INPUT 0xff
struct shell_config {
    /* data need to be filled by user */
    char *progname;
    FILE *input;
    char *prompt;
    void *cookie;  /** point to custom data for commands */
    unsigned int flags;
    STAILQ_HEAD(inputs_list, shell_input) inputs_list;
    int inputs_count;
    struct commands_t *commands;
    struct driver_t *drivers;
    sighandler_t signal_event_hook;

    /* internal data */
    int   shell_quit;
    int   sig_cancel;
    char *shell_input;
    char *history_file;
};

void rl_callback(char* line);
void shell_init(struct shell_config *sc);
void shell_init_input_argv(struct shell_config *sc, int argc, char **argv);
void shell_deinit(struct shell_config *sc);
int  shell_handle(struct shell_config *sc);
void shell_show_help(struct shell_config *sc, char *name);

void shell_forced_update_display(struct shell_config *sc);
void shell_update_prompt(struct shell_config *sc, char *fmt, ...);

void shell_input_init(struct shell_config *sc);
void shell_input_init_current(struct shell_config *sc);
int  shell_input_add(struct shell_config *sc, int flags, ...);
struct shell_input* shell_input_next(struct shell_config *sc);

int is_interactive_mode(struct shell_config *sc);

#endif

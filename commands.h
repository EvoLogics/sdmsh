#ifndef COMMANDS_H
#define COMMANDS_H

#define ARG_LONG(name, str_val, out, range_expr) do {             \
    long arg;                                                     \
    errno = 0;                                                    \
    arg = strtol(str_val, NULL, 0);                               \
    if ((errno == ERANGE && (arg == LONG_MAX || arg == LONG_MIN)) \
        || (errno != 0 && arg == 0)) {                            \
        fprintf (stderr, name": must be a digit\n");              \
        return 0;                                                 \
    }                                                             \
    if (!(range_expr)) {                                          \
        fprintf (stderr, name": must be in range '%s'\n", #range_expr);     \
        return 0;                                                 \
    }                                                             \
    out = arg;                                                    \
} while(0)

typedef int (*command_handler)(char *argv[], int argc);

struct commands_t {
    char *name;
    command_handler handler;
    char *help;
    char *usage;
};

extern struct commands_t commands[];

#endif


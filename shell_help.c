#include <stdio.h>
#include <string.h>    /* strcmp() */

#include <shell.h>
#include <shell_help.h>

void shell_help_show_drivers(struct shell_config *sc, char *shift)
{
    struct driver_t *drv;

    printf("%s<driver> is:\n", shift);
    for (drv = sc->drivers; drv->name != NULL; drv++)
         printf("\t%s%s.\n\t%s%s\n\n", shift, drv->usage, shift, drv->help);
}

void shell_help_show(struct shell_config *sc, char *name)
{
    struct commands_t *cmd;
    for (cmd = sc->commands; cmd->name != NULL; cmd++) {
        if (!name || !strcmp(cmd->name, name)) {
            printf ("%-10s  -  %s\n", cmd->name, cmd->help);
            printf ("%-10s      Usage: %s\n", " ", cmd->usage ? cmd->usage : cmd->name);
            if (name) {
                if (cmd->flags & SCF_USE_DRIVER)
                    shell_help_show_drivers(sc, "\t\t");
                break;
            }
        }
    }

    if (!name)
        shell_help_show_drivers(sc, "");

    if (name && cmd->name == NULL) {
        fprintf(stderr, "Unknown topic: %s\n", name);
    }
}


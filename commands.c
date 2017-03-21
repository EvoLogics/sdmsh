#include <stdio.h>
#include <stdlib.h>    /* strtol() */
#include <errno.h>
#include <arpa/inet.h> /* inet_addr() */
#include <limits.h>    /* LONG_MAX  */
#include <unistd.h>    /* write() */
#include <err.h>       /* warn() */
#include <string.h>    /* strcmp() */

#include <readline/readline.h>

#include <shell.h>
#include <commands.h>
#include <logger.h>

#include <sdm.h>

int command_help(char *argv[], int argc);
int command_config(char *argv[], int argc);
int command_stop(char *argv[], int argc);
int command_ref(char *argv[], int argc);
int command_tx(char *argv[], int argc);
int command_rx(char *argv[], int argc);

struct commands_t commands[] = {
    {"help",   command_help,   "This help", "help [command]"}
   ,{"config", command_config, "Config SDM command.", "config <threshold> <gain> <source level>" }
   ,{"stop",   command_stop,   "Stop SDM command.", NULL}
   ,{"ref",    command_ref,    "Update reference signal.", "ref <file name>"}
   ,{"tx",     command_tx,     "Send signal.", "tx <file name>"}
   ,{"rx",     command_rx,     "Receive signal. Sample number can be 0", "rx <sample number> <file name>"}
   ,{NULL}
};

void show_help(char *name)
{
    struct commands_t *cmd;
    for (cmd = commands; cmd->name != NULL; cmd++) {
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
int command_help(char *argv[], int argc)
{
    argc = argc;
    show_help(argv[1]);
    return 0;
}

int command_config(char *argv[], int argc)
{
    uint16_t threshold;
    uint8_t gain, srclvl;

    if (argc != 4) {
        show_help(argv[0]);
        return -1;
    }

    ARG_LONG("threshold", argv[1], threshold, arg >= 1 && arg <= 4095);
    ARG_LONG("gain", argv[2], gain, arg >= 0 && arg <= 1);
    ARG_LONG("source level", argv[3], srclvl, arg >= 0 && arg <= 3);
    sdm_send_cmd(sockfd, SDM_CMD_CONFIG, threshold, gain, srclvl);
    smd_rcv_idle_state();

    return 0;
}

int command_stop(char *argv[], int argc)
{
    argv = argv;
    if (argc != 1) {
        show_help(argv[0]);
        return -1;
    }

    sdm_send_cmd(sockfd, SDM_CMD_STOP);
    smd_rcv_idle_state();

    return 0;
}

int command_ref(char *argv[], int argc)
{
    char  *data;
    size_t len;

    if (argc != 2) {
        show_help(argv[0]);
        return -1;
    }

    data = sdm_load_samples(argv[1], &len);
    logger (INFO_LOG, "read %d samles\n", len / 2);

    if (data) {
        if (len != 1024 * 2) {
            fprintf (stderr, "Error reference signal must be 1024 samples\n");
            return -1;
        }
        sdm_send_cmd(sockfd, SDM_CMD_REF, data, len);
        free(data);
    }

    smd_rcv_idle_state();
    return 0;
}

int command_tx(char *argv[], int argc)
{
    char  *data;
    size_t len;

    if (argc != 2) {
        show_help(argv[0]);
        return -1;
    }

    data = sdm_load_samples(argv[1], &len);
    logger (INFO_LOG, "read %d samles\n", len / 2);

    if (data) {
        size_t rest = len % (1024*2);
        if (rest) {
            rest = 2*1024 - rest;
            logger(WARN_LOG, "Warning: signal samples number %d do not divisible by 1024 samples. Zero padding added\n", len/2);
            data = realloc(data, len + rest);
            memset(data + len, 0, rest);
            len += rest;
        }
        sdm_send_cmd(sockfd, SDM_CMD_TX, data, len);
        free(data);
    }

    smd_rcv_idle_state();
    return 0;
}

int command_rx(char *argv[], int argc)
{
    long nsamples = 0;
    FILE *fp;

    if (argc != 3) {
        show_help(argv[0]);
        return -1;
    }

    /* 16776192 == 0xfffffc maximum 24 bit digit rounded to 1024 */
    ARG_LONG("samples number", argv[1], nsamples, arg >= 0 && arg <= 16776192);

    if (nsamples % 1024) {
        long old = nsamples;
        nsamples += 1024 - nsamples % 1024;
        logger(WARN_LOG, "Warning: signal samples number %ld do not divisible by 1024 samples. Expand to %ld\n"
                , old, nsamples);
    }

    /* truncate file if exist */
    if ((fp = fopen(argv[2], "w")) == NULL) {
        logger(ERR_LOG, "Error cannot open %s file: %s\n", argv[2], strerror(errno));
        return -1;
    }
    fclose(fp);

    sdm_rcv.filename = strdup(argv[2]);
    sdm_send_cmd(sockfd, SDM_CMD_RX, nsamples);
    /* rl_message("Waiting for receiving %ld samples to file %s\n", nsamples, sdm_rcv.filename); */
    
    return 0;
}

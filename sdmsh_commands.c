#include <stdio.h>
#include <stdlib.h>    /* strtol() */
#include <errno.h>
#include <arpa/inet.h> /* inet_addr() */
#include <limits.h>    /* LONG_MAX  */
#include <unistd.h>    /* write() */
#include <err.h>       /* warn() */
#include <string.h>    /* strcmp() */

#include <readline/readline.h>

#include <sdm.h>
#include <sdmsh_commands.h>

int sdmsh_cmd_help       (struct shell_config *sc, char *argv[], int argc);
int sdmsh_cmd_config     (struct shell_config *sc, char *argv[], int argc);
int sdmsh_cmd_stop       (struct shell_config *sc, char *argv[], int argc);
int sdmsh_cmd_ref        (struct shell_config *sc, char *argv[], int argc);
int sdmsh_cmd_tx         (struct shell_config *sc, char *argv[], int argc);
int sdmsh_cmd_rx         (struct shell_config *sc, char *argv[], int argc);
int sdmsh_cmd_usbl_config(struct shell_config *sc, char *argv[], int argc);
int sdmsh_cmd_usbl_rx    (struct shell_config *sc, char *argv[], int argc);

struct commands_t commands[] = {
    {"help",        sdmsh_cmd_help,       "This help", "help [command]"}
   ,{"config",      sdmsh_cmd_config,     "Config SDM command.", "config <threshold> <gain> <source level>" }
   ,{"usbl_config", sdmsh_cmd_usbl_config,"Config SDM USBL command.", "usbl_config <delay> <samples> <gain> <sample_rate>"}
   ,{"stop",        sdmsh_cmd_stop,       "Stop SDM command.", NULL}
   ,{"ref",         sdmsh_cmd_ref,        "Update reference signal.", "ref <file name>"}
   ,{"tx",          sdmsh_cmd_tx,         "Send signal.", "tx <file name>"}
   ,{"rx",          sdmsh_cmd_rx,         "Receive signal. Sample number can be 0", "rx <number of samples> <file name>"}
   ,{"usbl_rx",     sdmsh_cmd_usbl_rx,    "Receive signal from USBL channel.", "usbl_rx <channel> <number of samples> <file name>"}
   ,{NULL}
};

int sdmsh_cmd_help(struct shell_config *sc, char *argv[], int argc)
{
    argc = argc;
    shell_show_help(sc, argv[1]);
    return 0;
}

int sdmsh_cmd_config(struct shell_config *sc, char *argv[], int argc)
{
    uint16_t threshold;
    uint8_t gain, srclvl;
    sdm_session_t *ss = sc->cookie;

    if (argc != 4) {
        shell_show_help(sc, argv[0]);
        return -1;
    }

    ARG_LONG("threshold", argv[1], threshold, arg >= 1 && arg <= 4095);
    ARG_LONG("gain", argv[2], gain, arg >= 0 && arg <= 1);
    ARG_LONG("source level", argv[3], srclvl, arg >= 0 && arg <= 3);
    sdm_cmd(ss, SDM_CMD_CONFIG, threshold, gain, srclvl);
    sdm_set_idle_state(ss);

    return 0;
}

int sdmsh_cmd_usbl_config(struct shell_config *sc, char *argv[], int argc)
{
    uint16_t delay, samples;
    uint8_t gain, sample_rate;
    sdm_session_t *ss = sc->cookie;

    if (argc != 5) {
        shell_show_help(sc, argv[0]);
        return -1;
    }
    
    ARG_LONG("delay", argv[1], delay, arg >= 0 && arg <= 65535);
    ARG_LONG("number of samples", argv[2], samples, arg >= 1024 && arg <= 51200 && (arg % 1024) == 0);
    ARG_LONG("gain", argv[3], gain, arg >= 0 && arg <= 13);
    ARG_LONG("sample_rate", argv[4], sample_rate, arg >= 0 && arg <= 6);

    sdm_cmd(ss, SDM_CMD_USBL_CONFIG, delay, samples, gain, sample_rate);
    sdm_set_idle_state(ss);

    return 0;
}

int sdmsh_cmd_stop(struct shell_config *sc, char *argv[], int argc)
{
    sdm_session_t *ss = sc->cookie;

    argv = argv;
    if (argc != 1) {
        shell_show_help(sc, argv[0]);
        return -1;
    }

    sdm_cmd(ss, SDM_CMD_STOP);
    sdm_set_idle_state(ss);

    return 0;
}

int sdmsh_cmd_ref(struct shell_config *sc, char *argv[], int argc)
{
    uint16_t  *data;
    size_t len;
    sdm_session_t *ss = sc->cookie;

    if (argc != 2) {
        shell_show_help(sc, argv[0]);
        return -1;
    }

    data = sdm_load_samples(argv[1], &len);
    if (data == NULL) {
        sdm_set_idle_state(ss);
        return 1;
    }

    logger (INFO_LOG, "read %d samles\n", len);

    if (len != 1024) {
        logger (WARN_LOG, "Error reference signal must be 1024 samples\n");
        return -1;
    }
    sdm_cmd(ss, SDM_CMD_REF, data, len);
    free(data);

    sdm_set_idle_state(ss);
    return 0;
}

int sdmsh_cmd_tx(struct shell_config *sc, char *argv[], int argc)
{
    uint16_t  *data;
    size_t len;
    sdm_session_t *ss = sc->cookie;

    if (argc != 2) {
        shell_show_help(sc, argv[0]);
        return -1;
    }

    data = sdm_load_samples(argv[1], &len);
    if (data == NULL) {
        sdm_set_idle_state(ss);
        return 1;
    }

    logger (INFO_LOG, "read %d samles\n", len);

    size_t rest = len % (1024);
    if (rest) {
        rest = 1024 - rest;
        logger(WARN_LOG, "Warning: signal samples number %d do not divisible by 1024 samples. Zero padding added\n", len);
        data = realloc(data, (len + rest) * 2);
        if (data == NULL)
            err(1, "realloc()");
        memset(data + len, 0, rest * 2);
        len += rest;
    }
    sdm_cmd(ss, SDM_CMD_TX, data, len);
    free(data);

    sdm_set_idle_state(ss);
    return 0;
}

int sdmsh_cmd_rx(struct shell_config *sc, char *argv[], int argc)
{
    long nsamples = 0;
    FILE *fp;
    sdm_session_t *ss = sc->cookie;

    if (argc != 3) {
        shell_show_help(sc, argv[0]);
        return -1;
    }

    /* 16776192 == 0xfffffc maximum 24 bit digit rounded to 1024 */
    ARG_LONG("number of samples", argv[1], nsamples, arg >= 0 && arg <= 16776192);

    if (nsamples % 1024) {
        long old = nsamples;
        nsamples += 1024 - nsamples % 1024;
        logger(WARN_LOG, "Warning: signal number of samples %ld do not divisible by 1024 samples. Expand to %ld\n"
                , old, nsamples);
    }

    /* truncate file if exist */
    if ((fp = fopen(argv[2], "w")) == NULL) {
        logger(ERR_LOG, "Error cannot open %s file: %s\n", argv[2], strerror(errno));
        return -1;
    }
    fclose(fp);

    ss->filename = strdup(argv[2]);
    sdm_cmd(ss, SDM_CMD_RX, nsamples);
    /* rl_message("Waiting for receiving %ld samples to file %s\n", nsamples, ss->filename); */
    
    return 0;
}

int sdmsh_cmd_usbl_rx(struct shell_config *sc, char *argv[], int argc)
{
    uint8_t channel = 0;
    uint16_t samples = 0;
    FILE *fp;
    sdm_session_t *ss = sc->cookie;

    if (argc != 4) {
        shell_show_help(sc, argv[0]);
        return -1;
    }

    ARG_LONG("channel", argv[1], channel, arg >= 0 && arg <= 4);
    ARG_LONG("number of samples", argv[2], samples, arg >= 1024 && arg <= 51200 && (arg % 1024) == 0);
    
    /* truncate file if exist */
    if ((fp = fopen(argv[3], "w")) == NULL) {
        logger(ERR_LOG, "Error cannot open %s file: %s\n", argv[3], strerror(errno));
        return -1;
    }
    fclose(fp);

    ss->filename = strdup(argv[3]);
    sdm_cmd(ss, SDM_CMD_USBL_RX, channel, samples);
    
    return 0;
}

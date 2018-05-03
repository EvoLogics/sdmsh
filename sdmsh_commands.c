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

#include <stream.h>

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
   ,{"config",      sdmsh_cmd_config,     "Config SDM command.", "config <threshold> <gain> <source level> [<preamp_gain>]" }
   ,{"usbl_config", sdmsh_cmd_usbl_config,"Config SDM USBL command.", "usbl_config <delay> <samples> <gain> <sample_rate>"}
   ,{"stop",        sdmsh_cmd_stop,       "Stop SDM command.", NULL}
   ,{"ref",         sdmsh_cmd_ref,        "Update reference signal.", "ref [<drv>:]<params>"}
   ,{"tx",          sdmsh_cmd_tx,         "Send signal.", "tx [<number of samples>] [<driver>:]<parameter>"}
   ,{"rx",          sdmsh_cmd_rx,         "Receive signal [0 is inf].", "rx <number of samples> [<drv>:]<params>  [[<drv>:]<params>]"}
   ,{"usbl_rx",     sdmsh_cmd_usbl_rx,    "Receive signal from USBL channel.", "usbl_rx <channel> <number of samples> [<drv>:]<params>"}
   ,{NULL}
};

int sdmsh_stream_new(sdm_session_t *ss, int direction, char *parameter)
{
    char *arg = strdup(parameter);
    char *default_drv = "ascii";
    char *drv, *drv_param;
    sdm_stream_t *stream;
    /* argv[2]: driver:parameter */
    /* raw:rx.raw */

    if (ss->stream_cnt >= 2) {
        logger(ERR_LOG, "Too many streams open: %d\n", ss->stream_cnt);
        return -1;
    }
    if (strchr(parameter, ':')) {
        drv = strtok(parameter, ":");
        if (drv == NULL) {
            logger(ERR_LOG, "Output format error: %s\n", parameter);
            goto stream_new_error;
        }
        drv_param = strtok(NULL, "");
        if (drv_param == NULL) {
            logger(ERR_LOG, "Output parameter undefined\n");
            goto stream_new_error;
        }
    } else {
        drv = default_drv;
        drv_param = parameter;
    }
    /* if (ss->stream)  */
    /*     sdm_stream_free(ss->stream); */
    
    stream = sdm_stream_new(direction, drv, drv_param);
    if (stream == NULL) {
        logger(ERR_LOG, "Stream creation error\n");
        goto stream_new_error;
    }
    ss->stream[ss->stream_cnt] = stream;
    ss->stream_cnt++;
    free(arg);
    return 0;
  stream_new_error:
    free(arg);
    return -1;
}

int sdmsh_cmd_help(struct shell_config *sc, char *argv[], int argc)
{
    argc = argc;
    shell_show_help(sc, argv[1]);
    return 0;
}

int sdmsh_cmd_config(struct shell_config *sc, char *argv[], int argc)
{
    uint16_t threshold;
    uint8_t gain, srclvl, preamp_gain = 0;
    sdm_session_t *ss = sc->cookie;

    if (argc != 4 && argc != 5) {
        shell_show_help(sc, argv[0]);
        return -1;
    }

    ARG_LONG("threshold", argv[1], threshold, arg >= 1 && arg <= 4095);
    ARG_LONG("gain", argv[2], gain, arg >= 0 && arg <= 1);
    ARG_LONG("source level", argv[3], srclvl, arg >= 0 && arg <= 3);
    if (argc == 5) {
        ARG_LONG("preamp gain", argv[4], preamp_gain, arg >= 0 && arg <= 13);
    }
    sdm_cmd(ss, SDM_CMD_CONFIG, threshold, gain, srclvl, preamp_gain);
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
    int16_t  *data;
    size_t len = 1024;
    sdm_session_t *ss = sc->cookie;
    unsigned cnt;
    int rv, cmd; 

    if (argc != 2) {
        shell_show_help(sc, argv[0]);
        return -1;
    }
    sdm_free_streams(ss);
    
    if (sdmsh_stream_new(ss, STREAM_INPUT, argv[1])) {
        return -1;
    }
    if (sdm_stream_open(ss->stream[0])) {
        logger(ERR_LOG, "Stream open error: %s\n", sdm_stream_get_error(ss->stream[0]));
        return -1;
    }
    data = malloc(len * sizeof(int16_t));
    cnt = sdm_load_samples(ss, data, len);
    cmd = SDM_CMD_REF;
    if (cnt == len) {
        rv = sdm_cmd(ss, cmd, data, len);
    } else if (cnt > 0) {
        logger (WARN_LOG, "Padding reference signal to 1024 samples\n");
        memset(&data[cnt], 0, (len - cnt) * 2);
        rv = sdm_cmd(ss, cmd, data, len);
    } else {
        rv = -1;
    }
    free(data);
    sdm_set_idle_state(ss);
    return rv;
}

int sdmsh_cmd_tx(struct shell_config *sc, char *argv[], int argc)
{
    size_t len = 1024 * 2, cnt, cmd;
    int16_t *data;
    sdm_session_t *ss = sc->cookie;
    int rv;
    unsigned nsamples, passed = 0;
    int arg_id = 1;

    if (argc != 3 && argc != 2) {
        shell_show_help(sc, argv[0]);
        return -1;
    }
    if (argc == 3) {
        ARG_LONG("number of samples", argv[1], nsamples, arg >= 0 && arg <= 16776192);
        arg_id = 2;
    } else {
        nsamples = 0;
        arg_id = 1;
    }
    sdm_free_streams(ss);
    if (sdmsh_stream_new(ss, STREAM_INPUT, argv[arg_id])) {
        return -1;
    }
    if (nsamples == 0) {
        nsamples = sdm_stream_count(ss->stream[0]);
        if (nsamples == 0) {
            logger(ERR_LOG, "Zero samples\n");
            return -1;
        }
    }
    
    if (sdm_stream_open(ss->stream[0])) {
        logger(ERR_LOG, "Stream open error: %s\n", sdm_stream_get_error(ss->stream[0]));
        return -1;
    }
    data = malloc(len * sizeof(int16_t));

    cmd = SDM_CMD_TX;
    do {
        len = len < nsamples - passed ? len : nsamples - passed;
        cnt = sdm_load_samples(ss, data, len);
        if (cnt == 0) {
            cmd = SDM_CMD_STOP;
            break;
        }
        if (cnt == len) {
            rv = sdm_cmd(ss, cmd, nsamples, data, len);
            passed += len;
        } else if (cnt > 0) {
            memset(&data[cnt], 0, (len - cnt) * 2);
            rv = sdm_cmd(ss, cmd, nsamples, data, 1024 * ((cnt + 1023) / 1024));
            passed += 1024 * ((cnt + 1023) / 1024);
        } else {
            rv = -1;
        }
        cmd = SDM_CMD_TX_CONTINUE;
    } while (rv == 0 && passed < nsamples);
    free(data);
    sdm_set_idle_state(ss);
    return 0;
}

int sdmsh_cmd_rx(struct shell_config *sc, char *argv[], int argc)
{
    long nsamples = 0;
    /* FILE *fp; */
    sdm_session_t *ss = sc->cookie;
    int strm_cnt = argc - 2, i;

    if (argc != 3 && argc != 4) {
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

    sdm_free_streams(ss);
    for (i = 0; i < strm_cnt; i++) {
        if (sdmsh_stream_new(ss, STREAM_OUTPUT, argv[2 + i])) {
            break;
        }
        if (sdm_stream_open(ss->stream[i])) {
            logger(ERR_LOG, "Stream open error: %s\n", sdm_stream_get_error(ss->stream[i]));
            break;
        }
    }
    if (i != strm_cnt) {
        sdm_free_streams(ss);
        return -1;
    }
    sdm_cmd(ss, SDM_CMD_RX, nsamples);
    /* rl_message("Waiting for receiving %ld samples to file %s\n", nsamples, ss->filename); */
    
    return 0;
}

int sdmsh_cmd_usbl_rx(struct shell_config *sc, char *argv[], int argc)
{
    uint8_t channel = 0;
    uint16_t samples = 0;
    sdm_session_t *ss = sc->cookie;

    if (argc != 4) {
        shell_show_help(sc, argv[0]);
        return -1;
    }

    ARG_LONG("channel", argv[1], channel, arg >= 0 && arg <= 4);
    ARG_LONG("number of samples", argv[2], samples, arg >= 1024 && arg <= 51200 && (arg % 1024) == 0);

    sdm_free_streams(ss);
    if (sdmsh_stream_new(ss, STREAM_OUTPUT, argv[3])) {
        return -1;
    }
    if (sdm_stream_open(ss->stream[0])) {
        logger(ERR_LOG, "Stream open error: %s\n", sdm_stream_get_error(ss->stream[0]));
        return -1;
    }
    sdm_cmd(ss, SDM_CMD_USBL_RX, channel, samples);
    
    return 0;
}

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
#include <history.h>

#include <stream.h>

int sdmsh_cmd_help       (struct shell_config *sc, char *argv[], int argc);
int sdmsh_cmd_config     (struct shell_config *sc, char *argv[], int argc);
int sdmsh_cmd_stop       (struct shell_config *sc, char *argv[], int argc);
int sdmsh_cmd_ref        (struct shell_config *sc, char *argv[], int argc);
int sdmsh_cmd_tx         (struct shell_config *sc, char *argv[], int argc);
int sdmsh_cmd_rx         (struct shell_config *sc, char *argv[], int argc);
int sdmsh_cmd_usbl_config(struct shell_config *sc, char *argv[], int argc);
int sdmsh_cmd_usbl_rx    (struct shell_config *sc, char *argv[], int argc);
int sdmsh_cmd_systime    (struct shell_config *sc, char *argv[], int argc);
int sdmsh_cmd_usleep     (struct shell_config *sc, char *argv[], int argc);
int sdmsh_cmd_history    (struct shell_config *sc, char *argv[], int argc);

struct commands_t commands[] = {
     {"config",      sdmsh_cmd_config,      SF_NONE,       "config <threshold> <gain> <source level> [<preamp_gain>]", "Config SDM command." }
   , {"usbl_config", sdmsh_cmd_usbl_config, SF_NONE,       "usbl_config <delay> <samples> <gain> <sample_rate>", "Config SDM USBL command."}
   , {"stop",        sdmsh_cmd_stop,        SF_NONE,       "stop", "Stop SDM command."}
   , {"ref",         sdmsh_cmd_ref,         SF_USE_DRIVER, "ref [<number of samples>] [<driver>:]<params>", "Update reference signal."}
   , {"tx",          sdmsh_cmd_tx,          SF_USE_DRIVER, "tx [<number of samples>] [<driver>:]<parameter>", "Send signal."}
   , {"rx",          sdmsh_cmd_rx,          SF_USE_DRIVER, "rx <number of samples> [<driver>:]<params> [[<driver>:]<params>]", "Receive signal [0 is inf]."}
   , {"usbl_rx",     sdmsh_cmd_usbl_rx,     SF_USE_DRIVER, "usbl_rx <channel> <number of samples> [<driver>:]<params>", "Receive signal from USBL channel."}
   , {"systime",     sdmsh_cmd_systime,     SF_NONE,       "systime", "Request systime."}
   , {"usleep",      sdmsh_cmd_usleep,      SF_NO_HISTORY, "usleep <usec>", "Delay in usec."}
   , {"help",        sdmsh_cmd_help,        SF_NONE,       "help [command]", "This help"}
   , {"history",     sdmsh_cmd_history,     SF_NO_HISTORY, "history [number-lines]", "Display history. Optional [number-lines] by default is 10."}
   , {NULL}
};

struct driver_t drivers[] = {
    {"ascii:", SF_DRIVER_FILENAME, "ascii:<filename>", "This is default driver File format: float (-1.0 .. 1.0) or short interger as text line, one value per line" }
  , {"raw:",   SF_DRIVER_FILENAME, "raw:<filename>", "Binary format: int16_t per value" }
  , {"tcp:",   SF_DRIVER_NET,      "tcp:<connect|listen>:<ip>:<port>", "Opens TCP socket to send or receive data, int16_t per value" }
  , {NULL}
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

    ARGS_RANGE(argc == 4 || argc == 5);
    ARG_LONG("config: threshold", argv[1], threshold, arg >= 1 && arg <= 4095);
    ARG_LONG("config: gain", argv[2], gain, arg >= 0 && arg <= 1);
    ARG_LONG("config: source level", argv[3], srclvl, arg >= 0 && arg <= 3);
    if (argc == 5) {
        ARG_LONG("config: preamp gain", argv[4], preamp_gain, arg >= 0 && arg <= 13);
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

    ARGS_RANGE(argc == 5);
    ARG_LONG("usbl_config: delay", argv[1], delay, arg >= 0 && arg <= 65535);
    ARG_LONG("usbl_config: number of samples", argv[2], samples, arg >= 1024 && arg <= 51200 && (arg % 1024) == 0);
    ARG_LONG("usbl_config: gain", argv[3], gain, arg >= 0 && arg <= 13);
    ARG_LONG("usbl_config: sample_rate", argv[4], sample_rate, arg >= 0 && arg <= 6);

    sdm_cmd(ss, SDM_CMD_USBL_CONFIG, delay, samples, gain, sample_rate);
    sdm_set_idle_state(ss);

    return 0;
}

int sdmsh_cmd_stop(struct shell_config *sc, char *argv[], int argc)
{
    sdm_session_t *ss = sc->cookie;

    argv = argv;
    ARGS_RANGE(argc == 1);
    sdm_cmd(ss, SDM_CMD_STOP);
    sdm_set_idle_state(ss);

    return 0;
}

int sdmsh_cmd_ref(struct shell_config *sc, char *argv[], int argc)
{
    int16_t  *data;
    ssize_t len = 1024;
    sdm_session_t *ss = sc->cookie;
    int rc;
    int samples_count = len;

    ARGS_RANGE(argc == 2 || argc == 3);
    sdm_free_streams(ss);
    
    if (argc == 3) {
        ARG_LONG("ref: samples-count", argv[1], samples_count, arg >= 1 && arg <= 1024);
        argv++;
    }

    if (sdmsh_stream_new(ss, STREAM_INPUT, argv[1]))
        return -1;

    if (sdm_stream_open(ss->stream[0])) {
        logger(ERR_LOG, "ref: %s error %s\n", argv[1], sdm_stream_get_error(ss->stream[0]));
        return -1;
    }

    data = calloc(len, sizeof(int16_t));

    rc = sdm_load_samples(ss, data + len - samples_count, samples_count);

    if (rc < 0) {
        logger(ERR_LOG, "ref: %s error %s\n", argv[1], sdm_stream_get_error(ss->stream[0]));
        rc = -1;
    } else {
        if (rc != len) {
            logger (WARN_LOG, "ref: Padding before reference %d samples, to reference signal to 1024 samples\n", len - rc);
            if (samples_count == len) {
                memmove(data + len - rc, data, rc * sizeof(int16_t));
                memset(data, 0, (len - rc) * sizeof(int16_t));
            }
        }

        /* DUMP2LOG(ERR_LOG, (char *)data, len*2); */
        rc = sdm_cmd(ss, SDM_CMD_REF, data, len);
    }

    free(data);
    sdm_set_idle_state(ss);

    return rc;
}

int sdmsh_cmd_tx(struct shell_config *sc, char *argv[], int argc)
{
    size_t len = 1024 * 2, cnt, cmd;
    int16_t *data;
    sdm_session_t *ss = sc->cookie;
    int rc;
    unsigned nsamples = 0, passed = 0;

    ARGS_RANGE(argc == 3 || argc == 2);
    if (argc == 3) {
        ARG_LONG("tx: number of samples", argv[1], nsamples, arg >= 0 && arg <= 16776192);
        argv++;
    }

    sdm_free_streams(ss);
    if (sdmsh_stream_new(ss, STREAM_INPUT, argv[1])) {
        return -1;
    }
    if (nsamples == 0) {
        nsamples = sdm_stream_count(ss->stream[0]);
        if (nsamples == 0) {
            logger(ERR_LOG, "tx: Zero samples\n");
            return -1;
        }
    }
    
    if (sdm_stream_open(ss->stream[0])) {
        logger(ERR_LOG, "tx: %s error %s\n", argv[1], sdm_stream_get_error(ss->stream[0]));
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

        if (cnt == len && nsamples >= 1024) {
            rc = sdm_cmd(ss, cmd, nsamples, data, len);
            passed += len;
        } else if (cnt > 0) {
            memset(&data[cnt], 0, (len - cnt) * 2);
            rc = sdm_cmd(ss, cmd, nsamples, data, 1024 * ((cnt + 1023) / 1024));
            passed += 1024 * ((cnt + 1023) / 1024);
        } else {
            rc = -1;
        }
        cmd = SDM_CMD_TX_CONTINUE;
    } while (rc == 0 && passed < nsamples);
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

    ARGS_RANGE(argc == 3 || argc == 2);

    /* 16776192 == 0xfffffc maximum 24 bit digit rounded to 1024 */
    ARG_LONG("rx: number of samples", argv[1], nsamples, arg >= 0 && arg <= 16776192);

    if (nsamples % 1024) {
        long old = nsamples;
        nsamples += 1024 - nsamples % 1024;
        logger(WARN_LOG, "rx: Warning: signal number of samples %ld do not divisible by 1024 samples. Expand to %ld\n"
                , old, nsamples);
    }

    sdm_free_streams(ss);
    for (i = 0; i < strm_cnt; i++) {
        if (sdmsh_stream_new(ss, STREAM_OUTPUT, argv[2 + i])) {
            break;
        }
        if (sdm_stream_open(ss->stream[i])) {
            logger(ERR_LOG, "rx: %s error %s\n", argv[1], sdm_stream_get_error(ss->stream[0]));
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

    ARGS_RANGE(argc == 4);
    ARG_LONG("usbl_rx: channel", argv[1], channel, arg >= 0 && arg <= 4);
    ARG_LONG("usbl_rx: number of samples", argv[2], samples, arg >= 1024 && arg <= 51200 && (arg % 1024) == 0);

    sdm_free_streams(ss);
    if (sdmsh_stream_new(ss, STREAM_OUTPUT, argv[3])) {
        return -1;
    }
    if (sdm_stream_open(ss->stream[0])) {
        logger(ERR_LOG, "usbl_rx: %s error %s\n", argv[1], sdm_stream_get_error(ss->stream[0]));
        return -1;
    }
    sdm_cmd(ss, SDM_CMD_USBL_RX, channel, samples);
    
    return 0;
}

int sdmsh_cmd_systime(struct shell_config *sc, char *argv[], int argc)
{
    sdm_session_t *ss = sc->cookie;

    ARGS_RANGE(argc == 1);
    sdm_cmd(ss, SDM_CMD_SYSTIME);
    sdm_set_idle_state(ss);

    return 0;
}

int sdmsh_cmd_usleep(struct shell_config *sc, char *argv[], int argc)
{
    useconds_t usec;
    sc = sc;

    ARGS_RANGE(argc == 2);
    ARG_LONG("usleep: usec", argv[1], usec, arg >= 0);
    usleep(usec);

    return 0;
}

int sdmsh_cmd_history(struct shell_config *sc, char *argv[], int argc)
{
    int hist_num = 10;
    sc = sc;

    ARGS_RANGE(argc == 1 || argc == 2);
    if (argc == 2)
        ARG_LONG("history: number-lines", argv[1], hist_num, arg >= 0);

    shell_show_history(hist_num);

    return 0;
}

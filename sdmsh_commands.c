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
#include <janus/janus.h>
#include <sdmsh_commands.h>
#include <shell_history.h>
#include <shell_help.h>

#include <stream.h>

int sdmsh_cmd_help       (struct shell_config *sc, char *argv[], int argc);
int sdmsh_cmd_config     (struct shell_config *sc, char *argv[], int argc);
int sdmsh_cmd_stop       (struct shell_config *sc, char *argv[], int argc);
int sdmsh_cmd_ref        (struct shell_config *sc, char *argv[], int argc);
int sdmsh_cmd_tx         (struct shell_config *sc, char *argv[], int argc);
int sdmsh_cmd_rx         (struct shell_config *sc, char *argv[], int argc);
int sdmsh_cmd_rx_janus   (struct shell_config *sc, char *argv[], int argc);
int sdmsh_cmd_usbl_config(struct shell_config *sc, char *argv[], int argc);
int sdmsh_cmd_usbl_rx    (struct shell_config *sc, char *argv[], int argc);
int sdmsh_cmd_systime    (struct shell_config *sc, char *argv[], int argc);
int sdmsh_cmd_waitsyncin (struct shell_config *sc, char *argv[], int argc);
int sdmsh_cmd_usleep     (struct shell_config *sc, char *argv[], int argc);
int sdmsh_cmd_source     (struct shell_config *sc, char *argv[], int argc);
int sdmsh_cmd_history    (struct shell_config *sc, char *argv[], int argc);

struct commands_t commands[] = {
     {"config",      sdmsh_cmd_config,      SCF_NONE,       "config <threshold> <gain> <source level> [<preamp_gain>]", "Config SDM command." }
   , {"usbl_config", sdmsh_cmd_usbl_config, SCF_NONE,       "usbl_config <delay> <samples> <gain> <sample_rate>", "Config SDM USBL command."}
   , {"stop",        sdmsh_cmd_stop,        SCF_NONE,       "stop", "Stop SDM command."}
   , {"ref",         sdmsh_cmd_ref,         SCF_USE_DRIVER, "ref [<number of samples>] [<driver>:]<params>", "Update reference signal."}
   , {"tx",          sdmsh_cmd_tx,          SCF_USE_DRIVER, "tx [<number of samples>] [<driver>:]<parameter>", "Send signal."}
   , {"rx",          sdmsh_cmd_rx,          SCF_USE_DRIVER, "rx <number of samples> [<driver>:]<params> [[<driver>:]<params>]", "Receive signal [0 is inf]."}
   , {"rx_janus",    sdmsh_cmd_rx_janus,    SCF_USE_DRIVER, "rx_janus <number of samples> [<driver>:]<params> [[<driver>:]<params>]", "Receive signal [0 is inf]."}
   , {"usbl_rx",     sdmsh_cmd_usbl_rx,     SCF_USE_DRIVER, "usbl_rx <channel> <number of samples> [<driver>:]<params>", "Receive signal from USBL channel."}
   , {"systime",     sdmsh_cmd_systime,     SCF_NONE,       "systime", "Request systime."}
   , {"waitsyncin",  sdmsh_cmd_waitsyncin,  SCF_NONE,       "waitsyncin", "Wait SYNCIN message."}
   , {"usleep",      sdmsh_cmd_usleep,      SCF_NO_HISTORY, "usleep <usec>", "Delay in usec."}
   , {"source",      sdmsh_cmd_source,      SCF_NONE,       "source <source-file>", "Run commands from file."}
   , {"help",        sdmsh_cmd_help,        SCF_NONE,       "help [command]", "This help"}
   , {"history",     sdmsh_cmd_history,     SCF_NO_HISTORY, "history [number-lines]", "Display history. Optional [number-lines] by default is 10."}
   , {NULL, NULL, 0, NULL, NULL }
};

struct driver_t drivers[] = {
    {"ascii:", SCF_DRIVER_FILENAME, "ascii:<filename> or file extension \".dat\" or \".txt\""
        , "This is default driver File format: float (-1.0 .. 1.0) or short integer (-32768 .. 32767) as text line, one value per line."
          "\n        Empty lines and '#' or '//' comments is allowed." }
  , {"raw:",   SCF_DRIVER_FILENAME, "raw:<filename> or file extension \".raw\", \".bin\", \".dmp\" or \".fifo\"", "Binary format: int16_t per value" }
  , {"tcp:",   SCF_DRIVER_NET,      "tcp:<connect|listen>:<ip>:<port>", "Opens TCP socket to send or receive data, int16_t per value" }
  , {"popen:", SCF_DRIVER_SH_LINE,  "popen:\"command-line\"", "Call external program to send or receive data, int16_t per value" }
  , {NULL, 0, NULL, NULL }
};

int sdmsh_cmd_help(struct shell_config *sc, char *argv[], int argc)
{
    argc = argc;
    shell_help_show(sc, argv[1]);
    return 0;
}

int sdmsh_cmd_config(struct shell_config *sc, char *argv[], int argc)
{
    uint16_t threshold;
    uint8_t gain, srclvl, preamp_gain = 0;
    sdm_session_t *ss = sc->cookie;

    ARGS_RANGE(argc == 4 || argc == 5);
    SDM_CHECK_STR_ARG_LONG("config: threshold", argv[1], threshold, arg >= 0 && arg <= 4095);
    SDM_CHECK_STR_ARG_LONG("config: gain", argv[2], gain, arg >= 0 && arg <= 1);
    SDM_CHECK_STR_ARG_LONG("config: source level", argv[3], srclvl, arg >= 0 && arg <= 3);
    if (argc == 5) {
        SDM_CHECK_STR_ARG_LONG("config: preamp gain", argv[4], preamp_gain, arg >= 0 && arg <= 13);
    }
    sdm_send(ss, SDM_CMD_CONFIG, threshold, gain, srclvl, preamp_gain);
    sdm_set_idle_state(ss);

    return 0;
}

int sdmsh_cmd_usbl_config(struct shell_config *sc, char *argv[], int argc)
{
    uint16_t delay, samples;
    uint8_t gain, sample_rate;
    sdm_session_t *ss = sc->cookie;

    ARGS_RANGE(argc == 5);
    SDM_CHECK_STR_ARG_LONG("usbl_config: delay", argv[1], delay, arg >= 0 && arg <= 65535);
    SDM_CHECK_STR_ARG_LONG("usbl_config: number of samples", argv[2], samples, arg >= 1024 && arg <= 51200 && (arg % 1024) == 0);
    SDM_CHECK_STR_ARG_LONG("usbl_config: gain", argv[3], gain, arg >= 0 && arg <= 13);
    SDM_CHECK_STR_ARG_LONG("usbl_config: sample_rate", argv[4], sample_rate, arg >= 0 && arg <= 6);

    sdm_send(ss, SDM_CMD_USBL_CONFIG, delay, samples, gain, sample_rate);
    sdm_set_idle_state(ss);

    return 0;
}

int sdmsh_cmd_stop(struct shell_config *sc, char *argv[], int argc)
{
    sdm_session_t *ss = sc->cookie;

    argv = argv;
    ARGS_RANGE(argc == 1);
    sdm_send(ss, SDM_CMD_STOP);
    sdm_set_idle_state(ss);

    return 0;
}

int sdmsh_cmd_ref(struct shell_config *sc, char *argv[], int argc)
{
    uint16_t  *data;
    ssize_t len = 1024;
    sdm_session_t *ss = sc->cookie;
    int rc;
    int samples_count = len;
    stream_t* stream;

    ARGS_RANGE(argc == 2 || argc == 3);
    if (argc == 3) {
        SDM_CHECK_STR_ARG_LONG("ref: samples-count", argv[1], samples_count, arg >= 1 && arg <= 1024);
        argv++;
    }

    stream = stream_new(STREAM_INPUT, argv[1]);
    if (!stream)
        return -1;

    if (stream_open(stream)) {
        if (stream_get_errno(stream) == EINTR)
            logger(WARN_LOG, "ref: opening %s was interrupted\n", argv[1]);
        else
            logger(ERR_LOG, "ref: open error %s\n", stream_strerror(stream));
        return -1;
    }

    data = calloc(len, sizeof(uint16_t));

    rc = stream_read(stream, data + len - samples_count, samples_count);

    if (rc < 0) {
        logger(ERR_LOG, "ref: read error %s\n", stream_strerror(stream));
        rc = -1;
    } else {
        if (rc != len) {
            logger (WARN_LOG, "ref: Padding before reference %d samples, to reference signal to 1024 samples\n", len - rc);
            if (samples_count == len) {
                memmove(data + len - rc, data, rc * sizeof(uint16_t));
                memset(data, 0, (len - rc) * sizeof(uint16_t));
            }
        }

        /* DUMP2LOG(ERR_LOG, (char *)data, len*2); */
        rc = sdm_send(ss, SDM_CMD_REF, data, len);
    }

    stream_close(stream);
    stream_free(stream);
    free(data);
    sdm_set_idle_state(ss);

    return rc;
}

int sdmsh_cmd_tx(struct shell_config *sc, char *argv[], int argc)
{
    ssize_t len = 1024 * 2, cnt;
    uint16_t *data, cmd;
    sdm_session_t *ss = sc->cookie;
    int rc;
    unsigned nsamples = 0, passed = 0;
    stream_t* stream;

    ARGS_RANGE(argc == 3 || argc == 2);
    if (argc == 3) {
        SDM_CHECK_STR_ARG_LONG("tx: number of samples", argv[1], nsamples, arg >= 0 && arg <= 16776192);
        argv++;
    }

    streams_clean(&ss->streams);
    stream = streams_add_new(&ss->streams, STREAM_INPUT, argv[1]);
    if (!stream)
        return -1;

    if (nsamples == 0) {
        rc = stream_count(stream);
        if (rc < 0) {
            logger(ERR_LOG, "tx: stream count error %s\n", stream_strerror(stream));
            return -1;
        } else if (rc == 0) {
            logger(ERR_LOG, "tx: Zero samples. "
                  "In case sample source is stream (tcp or named pipe), "
                  "TX command with 0 samples as parameter, do not supported.\n");
            return -1;
        }
        nsamples = rc;
    }
    
    if (stream_open(stream)) {
        if (stream_get_errno(stream) == EINTR)
            logger(WARN_LOG, "tx: opening %s was interrupted\n", argv[1]);
        else
            logger(ERR_LOG, "tx: open error %s\n", stream_strerror(stream));
        return -1;
    }
    data = malloc(len * sizeof(uint16_t));

    cmd = SDM_CMD_TX;
    do {
        len = len < nsamples - passed ? len : nsamples - passed;
        cnt = stream_read(stream, data, len);

        if (cnt < 0) {
            logger(ERR_LOG, "tx: read error %s\n", stream_strerror(stream));
            break;
        }

        /* handle ctrl-c */
        if (cnt == 0 || sc->sig_cancel) {
            logger(INFO_LOG, "\rtx cancelled. Wait TX_STOP                     \n");
            break;
        }

        logger(DATA_LOG, "tx data %d / %d / %d samples  \r", nsamples, len, passed);

        if (cnt == len && nsamples >= 1024) {
            rc = sdm_send(ss, cmd, nsamples, data, len);
            passed += len;
        } else if (cnt > 0) {
            memset(&data[cnt], 0, (len - cnt) * 2);
            rc = sdm_send(ss, cmd, nsamples, data, 1024 * ((cnt + 1023) / 1024));
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

int sdmsh_cmd_rx_helper(struct shell_config *sc, char *argv[], int argc, int code)
{
    long nsamples = 0;
    /* FILE *fp; */
    sdm_session_t *ss = sc->cookie;
    int strm_cnt = 0, i;
    char **args_sink;

    /* 16776192 == 0xfffffc maximum 24 bit digit rounded to 1024 */
    if (code == SDM_CMD_RX) {
        ARGS_RANGE(argc >= 3);
        SDM_CHECK_STR_ARG_LONG("rx: number of samples", argv[1], nsamples, arg >= 0 && arg <= 16776192);
        strm_cnt  = argc - 2;
        args_sink = argv + 2;
    } else {
        if (sdm_janus_rx_check_executable() != 0)
            return -1;

        ARGS_RANGE(argc >= 0);
        strm_cnt  = argc - 1;
        args_sink = argv + 1;
    }

    if (nsamples % 1024) {
        long old = nsamples;
        nsamples += 1024 - nsamples % 1024;
        logger(WARN_LOG, "rx: Warning: signal number of samples %ld do not divisible by 1024 samples. Expand to %ld\n"
                , old, nsamples);
    }

    streams_clean(&ss->streams);
    for (i = 0; i < strm_cnt; i++) {
        stream_t* stream = streams_add_new(&ss->streams, STREAM_OUTPUT, args_sink[i]);
        if (!stream)
            break;

        if (stream_open(stream)) {
            if (stream_get_errno(stream) == EINTR)
                logger(WARN_LOG, "rx: opening %s was interrupted\n", args_sink[i]);
            else
                logger(ERR_LOG, "rx: %s error %s\n", stream_strerror(stream));
            break;
        }
    }

    if (i != strm_cnt) {
        streams_clean(&ss->streams);
        return -1;
    }

    sdm_send(ss, code, nsamples);
    /* rl_message("Waiting for receiving %ld samples to file %s\n", nsamples, ss->filename); */
    
    return 0;
}

int sdmsh_cmd_rx(struct shell_config *sc, char *argv[], int argc)
{
    return sdmsh_cmd_rx_helper(sc, argv, argc, SDM_CMD_RX);
}

int sdmsh_cmd_rx_janus(struct shell_config *sc, char *argv[], int argc)
{
    return sdmsh_cmd_rx_helper(sc, argv, argc, SDM_CMD_RX_JANUS);
}

int sdmsh_cmd_usbl_rx(struct shell_config *sc, char *argv[], int argc)
{
    uint8_t channel = 0;
    uint16_t samples = 0;
    sdm_session_t *ss = sc->cookie;
    stream_t* stream;

    ARGS_RANGE(argc == 4);
    SDM_CHECK_STR_ARG_LONG("usbl_rx: channel", argv[1], channel, arg >= 0 && arg <= 4);
    SDM_CHECK_STR_ARG_LONG("usbl_rx: number of samples", argv[2], samples, arg >= 1024 && arg <= 51200 && (arg % 1024) == 0);

    streams_clean(&ss->streams);
    stream = streams_add_new(&ss->streams, STREAM_OUTPUT, argv[3]);
    if (!stream)
        return -1;

    if (stream_open(stream)) {
        if (stream_get_errno(stream) == EINTR)
            logger(WARN_LOG, "rx: opening %s was interrupted\n", argv[1]);
        else
            logger(ERR_LOG, "usbl_rx: error %s\n", stream_strerror(stream));
        return -1;
    }
    sdm_send(ss, SDM_CMD_USBL_RX, channel, samples);
    
    return 0;
}

int sdmsh_cmd_systime(struct shell_config *sc, char *argv[], int argc)
{
    sdm_session_t *ss = sc->cookie;

    ARGS_RANGE(argc == 1);
    sdm_send(ss, SDM_CMD_SYSTIME);
    sdm_set_idle_state(ss);

    return 0;
}

int sdmsh_cmd_waitsyncin(struct shell_config *sc, char *argv[], int argc)
{
    sdm_session_t *ss = sc->cookie;

    argv = argv;
    ARGS_RANGE(argc == 1);
    sdm_set_idle_state(ss);
    ss->state = SDM_STATE_WAIT_SYNCIN;

    return 0;
}

int sdmsh_cmd_usleep(struct shell_config *sc, char *argv[], int argc)
{
    useconds_t usec;
    sc = sc;

    ARGS_RANGE(argc == 2);
    SDM_CHECK_STR_ARG_LONG("usleep: usec", argv[1], usec, arg >= 0);
    usleep(usec);

    return 0;
}

int sdmsh_cmd_history(struct shell_config *sc, char *argv[], int argc)
{
    int hist_num = 10;
    sc = sc;

    ARGS_RANGE(argc == 1 || argc == 2);
    if (argc == 2)
        SDM_CHECK_STR_ARG_LONG("history: number-lines", argv[1], hist_num, arg >= 0);

    shell_show_history(hist_num);

    return 0;
}

int sdmsh_cmd_source(struct shell_config *sc, char *argv[], int argc)
{
    int rc;

    ARGS_RANGE(argc == 2);

    rc = shell_input_add(sc, SHELL_INPUT_PUT_HEAD|SHELL_INPUT_TYPE_FILE, argv[1]);

    if (rc == -1)
        fprintf(stderr, "source: %s error \"%s\": ", argv[1], strerror(errno));
    else if (rc == -2)
        fprintf(stderr, "Too many inputs. Maximum %d", SHELL_MAX_INPUT);

    shell_input_init_current(sc);

    return rc;
}

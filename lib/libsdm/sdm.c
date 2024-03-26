#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>     /* write() */
#include <err.h>        /* err() */
#include <sys/types.h>  /* socket() */
#include <sys/socket.h>
#include <sys/stat.h>
#include <netdb.h>      /* getaddrinfo()() */
#include <string.h>     /* memset() */
#include <stdarg.h>
#include <errno.h>
#include <stdlib.h>     /* stdlib() */
#include <assert.h>
#include <limits.h>     /* SHORT_MAX  */
#include <sys/time.h>   /* struct timeval  */

#include <sdm.h>

#include <stream.h>
#include <error.h>
#include <janus/janus.h>

#define ADD_TO_DATA_VAL16bit(data, data_size, data_offset, val) \
    ADD_TO_DATA_VAL(2, data, data_size, data_offset, val)

#define ADD_TO_DATA_VAL(val_size, data, data_size, data_offset, val) do { \
    if (data_offset * val_size == data_size) { \
        if (data_size < 1024*val_size) {       \
            data_size = 1024*val_size;         \
        } else {                               \
            data_size <<= 1;                   \
        }                                      \
        data = realloc(data, data_size);       \
        assert(data != NULL);                  \
    }                                          \
    data[data_offset++] = (val);               \
} while(0)

unsigned long log_level = FATAL_LOG | ERR_LOG | WARN_LOG | INFO_LOG;

sdm_session_t* sdm_connect(char *host, int port)
{
    sdm_session_t* ss;
    struct addrinfo hints, *resolv, *rp;
    int sockfd;
    char buf_host[NI_MAXHOST];
    char buf_port[NI_MAXSERV];

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(buf_port, sizeof(buf_port), "%d", port);

    if (getaddrinfo(host, buf_port, &hints, &resolv) != 0)
        return NULL;

    for (rp = resolv; rp != NULL; rp = rp->ai_next) {
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd < 0)
            continue;

        if (getnameinfo(rp->ai_addr, rp->ai_addrlen, buf_host, sizeof (buf_host), buf_port, sizeof(buf_port), NI_NUMERICHOST) < 0) {
            rp = NULL;
            break;
        }
        logger(DEBUG_LOG, "Try connect to %s:%s..\n", buf_host, buf_port);

        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) >= 0)
            break; /* success */

        close(sockfd);
    }

    freeaddrinfo(resolv);

    if (rp == NULL)
        return NULL;

    ss = calloc(1, sizeof(sdm_session_t));
    if (ss == NULL)
        return NULL;

    ss->sockfd  = sockfd;
    ss->state   = SDM_STATE_INIT;
    ss->timeout = SDM_DEFAULT_TIMEOUT;

    return ss;
}

void sdm_close(sdm_session_t *ss)
{
    close(ss->sockfd);
    streams_clean(&ss->streams);

    if (ss->rx_data)
        free(ss->rx_data);
    if (ss->cmd)
        free(ss->cmd);
    free(ss);
}

void sdm_pack_cmd(sdm_pkt_t *cmd, char *buf)
{
    memcpy(&buf[SDM_PKT_T_OFFSET_MAGIC], &cmd->magic, sizeof(cmd->magic));
    memcpy(&buf[SDM_PKT_T_OFFSET_CMD],   &cmd->cmd,   sizeof(cmd->cmd));
    switch (cmd->cmd) {
        case SDM_CMD_CONFIG: { memcpy(&buf[SDM_PKT_T_OFFSET_THRESHOLD], &cmd->threshold,       sizeof(cmd->threshold));
                               memcpy(&buf[SDM_PKT_T_OFFSET_GAIN_LVL],  &cmd->gain_and_srclvl, sizeof(cmd->gain_and_srclvl));
                               break;
                             }
        case SDM_CMD_USBL_CONFIG:
        case SDM_CMD_RX_JANUS:
        case SDM_CMD_USBL_RX:
        case SDM_CMD_RX:     { memcpy(&buf[SDM_PKT_T_OFFSET_RX_LEN],    &cmd->rx_len,          3); break; }
        default:             { memcpy(&buf[SDM_PKT_T_OFFSET_PARAM],     &cmd->param,           sizeof(cmd->param));  break; }
    }
    memcpy(&buf[SDM_PKT_T_OFFSET_DATA_LEN], &cmd->data_len, sizeof(cmd->data_len));
}

void sdm_pack_reply(sdm_pkt_t *cmd, char **buf_out)
{
    char *buf = malloc(SDM_PKT_T_SIZE);

    memcpy(&buf[SDM_PKT_T_OFFSET_MAGIC],    &cmd->magic,    sizeof(cmd->magic));
    memcpy(&buf[SDM_PKT_T_OFFSET_CMD],      &cmd->cmd,      sizeof(cmd->cmd));
    memcpy(&buf[SDM_PKT_T_OFFSET_PARAM],    &cmd->param,    sizeof(cmd->param));
    memcpy(&buf[SDM_PKT_T_OFFSET_DUMMY],    &cmd->dummy,    sizeof(cmd->dummy));
    memcpy(&buf[SDM_PKT_T_OFFSET_DATA_LEN], &cmd->data_len, sizeof(cmd->data_len));

    if (cmd->data_len != 0) {
        switch (cmd->cmd) {
            case SDM_REPLY_RX: case SDM_REPLY_USBL_RX:
                buf = realloc(cmd, SDM_PKT_T_SIZE + cmd->data_len * 2);
                memcpy(&buf[SDM_PKT_T_OFFSET_DATA], cmd->data, cmd->data_len * 2);
                break;
        }
    }
    *buf_out = buf;
}

void sdm_unpack_reply(sdm_pkt_t **cmd, char *buf)
{
    sdm_pkt_t *c = *cmd;

    memcpy(&c->magic,    &buf[SDM_PKT_T_OFFSET_MAGIC],    sizeof(c->magic));
    memcpy(&c->cmd,      &buf[SDM_PKT_T_OFFSET_CMD],      sizeof(c->cmd));
    memcpy(&c->param,    &buf[SDM_PKT_T_OFFSET_PARAM],    sizeof(c->param));
    memcpy(&c->dummy,    &buf[SDM_PKT_T_OFFSET_DUMMY],    sizeof(c->dummy));
    memcpy(&c->data_len, &buf[SDM_PKT_T_OFFSET_DATA_LEN], sizeof(c->data_len));

    if (c->data_len == 0)
        return;

    switch (c->cmd) {
        case SDM_REPLY_RX: case SDM_REPLY_USBL_RX:
        case SDM_REPLY_SYSTIME:
            c = realloc(c, sizeof(sdm_pkt_t) + c->data_len * 2);
            memcpy(c->data, &buf[SDM_PKT_T_OFFSET_DATA], c->data_len * 2);
            *cmd = c;
            break;
    }
}

int sdm_send(sdm_session_t *ss, int cmd_code, ...)
{
    va_list ap;
    int n;
    char *data;
    int data_len = 0;
    char *cmd_raw;
    sdm_pkt_t *cmd;

    if (ss == NULL)
        return -1;
    cmd_raw  = calloc(1, SDM_PKT_T_SIZE);
    cmd = calloc(1, sizeof(sdm_pkt_t));

    cmd->magic = SDM_PKG_MAGIC;
    cmd->cmd = cmd_code;

    switch (cmd_code) {
        case SDM_CMD_STOP:
        case SDM_CMD_SYSTIME:
            break;
        case SDM_CMD_CONFIG: {
            uint16_t preamp_gain;
            
            va_start(ap, cmd_code);
            cmd->threshold        = va_arg(ap, int);
            cmd->gain_and_srclvl  = va_arg(ap, int) << 7;
            cmd->gain_and_srclvl |= va_arg(ap, int);
            preamp_gain = (va_arg(ap, int) & 0xf) << 12;
            va_end(ap);
            data_len = cmd->data_len = 1;
            cmd_raw = realloc(cmd_raw, SDM_PKT_T_SIZE + cmd->data_len * 2);
            memcpy(&cmd_raw[SDM_PKT_T_OFFSET_DATA], &preamp_gain, 2);
            break;
        }
        case SDM_CMD_USBL_CONFIG:
        {
            uint32_t delay, samples;
            uint32_t gain, sample_rate;
            
            va_start(ap, cmd_code);
            delay = va_arg(ap, int);
            samples = va_arg(ap, int);
            gain = va_arg(ap, int);
            sample_rate = va_arg(ap, int);
            va_end(ap);
            
            cmd->rx_len = (gain << 4) + (sample_rate << 1);

            data_len = cmd->data_len = 4;
            cmd_raw = realloc(cmd_raw, SDM_PKT_T_SIZE + cmd->data_len * 2);

            memcpy(&cmd_raw[SDM_PKT_T_OFFSET_DATA],     &delay,   4);
            memcpy(&cmd_raw[SDM_PKT_T_OFFSET_DATA + 4], &samples, 4);
            break;
        }
        case SDM_CMD_TX_CONTINUE:
            va_start(ap, cmd_code);
            va_arg(ap, unsigned);
            data = va_arg(ap, char *);
            data_len = va_arg(ap, int);
            va_end(ap);
            break;
        case SDM_CMD_TX:
        {
            char *d;

            va_start(ap, cmd_code);

            cmd->data_len = va_arg(ap, unsigned);
            d             = va_arg(ap, char *);
            data_len      = va_arg(ap, int);

            /* FIXME: quick fix. Padding up to 1024 samples here */
            cmd->data_len = ((cmd->data_len + 1023) / 1024) * 1024;
            cmd_raw = realloc(cmd_raw, SDM_PKT_T_SIZE + data_len * 2);
            memcpy(&cmd_raw[SDM_PKT_T_OFFSET_DATA], d, data_len * 2);

            va_end(ap);
            break;
        }
        case SDM_CMD_REF:
        {
            char *d;

            va_start(ap, cmd_code);

            d             = va_arg(ap, char *);
            data_len      = va_arg(ap, int);
            cmd->data_len = data_len;

            cmd_raw = realloc(cmd_raw, SDM_PKT_T_SIZE + data_len * 2);
            memset(&cmd_raw[SDM_PKT_T_OFFSET_DATA], 0, data_len * 2);
            memcpy(&cmd_raw[SDM_PKT_T_OFFSET_DATA], d, data_len * 2);

            va_end(ap);
            break;
        }
        case SDM_CMD_RX:
        case SDM_CMD_RX_JANUS:
        {
            va_start(ap, cmd_code);
            cmd->rx_len = va_arg(ap, int) & 0xffffff;
            va_end(ap);
            break;
        }
        case SDM_CMD_USBL_RX:
        {
            uint8_t channel;
            uint16_t samples;
            
            va_start(ap, cmd_code);
            channel = va_arg(ap, int);
            samples = va_arg(ap, int);
            va_end(ap);

            cmd->rx_len = samples + (channel << 21);
            break;
        }
        default:
            free(cmd);
            free(cmd_raw);
            return -1;
    }

    if (cmd_code == SDM_CMD_TX_CONTINUE) {
        if (data_len) {
            logger(TRACE_LOG, "tx cmd continue: %d samples              \n", data_len);
            n = write(ss->sockfd, data, data_len * 2);
        }
    } else {
        sdm_pack_cmd(cmd, cmd_raw);
        logger(INFO_LOG, "tx cmd %-6s: %d samples ", sdm_cmd_to_str(cmd->cmd), data_len);
        DUMP_SHORT(DEBUG_LOG, LGREEN, cmd_raw, SDM_PKT_T_SIZE + data_len * 2);
        logger(INFO_LOG, "\n");
        n = write(ss->sockfd, cmd_raw, SDM_PKT_T_SIZE + data_len * 2);
    }
    free(cmd);
    free(cmd_raw);

    if (n < 0) {
        warn("write(): ");
        return -1;
    }
    ss->state = SDM_STATE_WAIT_REPLY;

    return 0;
}

char* sdm_cmd_to_str(uint8_t cmd)
{
    switch (cmd) {
        case SDM_CMD_STOP:        return "STOP";
        case SDM_CMD_TX:          return "TX";
        case SDM_CMD_TX_CONTINUE: return "TX_CONTINUE";
        case SDM_CMD_RX:          return "RX";
        case SDM_CMD_RX_JANUS:    return "RX_JANUS";
        case SDM_JANUS_DETECTED:  return "JANUS_DETECTED";
        case SDM_CMD_REF:         return "REF";
        case SDM_CMD_CONFIG:      return "CONFIG";
        case SDM_CMD_USBL_CONFIG: return "USBL_CONFIG";
        case SDM_CMD_USBL_RX:     return "USBL_RX";
        case SDM_CMD_SYSTIME:     return "SYSTIME";
        default: return "???";
    }
}

char* sdm_reply_to_str(uint8_t cmd)
{
    switch (cmd) {
        case SDM_REPLY_STOP:    return "STOP";
        case SDM_REPLY_RX:      return "RX";
        case SDM_REPLY_RX_JANUS:return "RX_JANUS";
        case SDM_REPLY_JANUS_DETECTED: return "JANUS_DETECTED";
        case SDM_REPLY_USBL_RX: return "USBL_RX";
        case SDM_REPLY_SYSTIME: return "SYSTIME";
        case SDM_REPLY_SYNCIN:  return "SYNCIN";
        case SDM_REPLY_BUSY:    return "BUSY";
        case SDM_REPLY_REPORT:  return "REPORT";
        default: return "???";
    }
}

int sdm_is_async_reply(uint8_t cmd)
{
    switch (cmd) {
        case SDM_REPLY_SYNCIN:
        case SDM_JANUS_DETECTED:  
            return 1;
    }
    return 0;
}

char* sdm_reply_report_to_str(uint8_t cmd)
{
    switch (cmd) {
        case SDM_REPLY_REPORT_NO_SDM_MODE: return "NO_SDM_MODE";
        case SDM_REPLY_REPORT_TX_STOP:     return "TX_STOP";
        case SDM_REPLY_REPORT_RX_STOP:     return "RX_STOP";
        case SDM_REPLY_REPORT_REF:         return "REF";
        case SDM_REPLY_REPORT_CONFIG:      return "CONFIG";
        case SDM_REPLY_REPORT_USBL_CONFIG: return "USBL_CONFIG";
        case SDM_REPLY_REPORT_USBL_RX_STOP:return "USBL_RX_STOP";
        case SDM_REPLY_REPORT_DROP:        return "DROP";
        case SDM_REPLY_REPORT_SYSTIME:     return "SYSTIME";
        case SDM_REPLY_REPORT_UNKNOWN:     return "UNKNOWN";
        default: return "???";
    }
}

int sdm_show(sdm_session_t *ss, sdm_pkt_t *cmd)
{
    char *buf;

    logger((sdm_is_async_reply(cmd->cmd) ? ASYNC_LOG : INFO_LOG)
            , "\rrx cmd %-6s: ", sdm_reply_to_str(cmd->cmd));
    sdm_pack_reply(cmd, &buf);
    DUMP_SHORT(DEBUG_LOG, YELLOW, buf, SDM_PKT_T_SIZE);
    free(buf);

    switch (cmd->cmd) {
        case SDM_REPLY_RX:
        case SDM_REPLY_RX_JANUS:
        case SDM_REPLY_USBL_RX:
            /* RX do not return data_len. No need to dump data */
        case SDM_REPLY_STOP:
            /* spaces need to clean last message 'recv %d samples' */
            logger(INFO_LOG, "          \n");
            break;
        case SDM_REPLY_BUSY:
            logger(INFO_LOG, "%d\n", cmd->param);
            break;
        case SDM_REPLY_SYSTIME:
            if (cmd->data_len == 8) {
                logger (INFO_LOG, "current_time = %u, tx_time = %u, rx_time = %u, syncin_time = %u\n"
                       , cmd->current_time, cmd->tx_time, cmd->rx_time, cmd->syncin_time);
            } else {
                logger (INFO_LOG, "current_time = %u, tx_time = %u, rx_time = %u\n"
                       , cmd->current_time, cmd->tx_time, cmd->rx_time);
            }
            break;
        case SDM_REPLY_SYNCIN:
            logger(ASYNC_LOG, "\n");
            break;
        case SDM_REPLY_JANUS_DETECTED: {
            int32_t janus_nshift;
            float janus_doppler;
            memcpy(&janus_nshift, &ss->rx_data[0], 4);
            memcpy(&janus_doppler, &ss->rx_data[4], 4);
            logger (INFO_LOG, " janus_nshift = %d, janus_doppler = %f\n", janus_nshift, janus_doppler);
            break;
        }
        case SDM_REPLY_REPORT:
            switch (cmd->param) {
                case SDM_REPLY_REPORT_NO_SDM_MODE: logger(INFO_LOG, " %s\n", sdm_reply_report_to_str(SDM_REPLY_REPORT_NO_SDM_MODE)); break;
                case SDM_REPLY_REPORT_TX_STOP:     logger(INFO_LOG, " %s after %d samples\n", sdm_reply_report_to_str(cmd->param), cmd->data_len); break;
                case SDM_REPLY_REPORT_RX_STOP:     logger(INFO_LOG, " %s after %d samples\n", sdm_reply_report_to_str(cmd->param), cmd->data_len); break;
                case SDM_REPLY_REPORT_REF:         logger(INFO_LOG, " %s %s\n", sdm_reply_report_to_str(cmd->param),    cmd->data_len ? "done":"fail"); break;
                case SDM_REPLY_REPORT_CONFIG:      logger(INFO_LOG, " %s %s\n", sdm_reply_report_to_str(cmd->param), cmd->data_len ? "done":"fail"); break;
                case SDM_REPLY_REPORT_USBL_CONFIG: logger(INFO_LOG, " %s %s\n", sdm_reply_report_to_str(cmd->param), cmd->data_len ? "done":"fail"); break;
                case SDM_REPLY_REPORT_USBL_RX_STOP:logger(INFO_LOG, " %s %s\n", sdm_reply_report_to_str(cmd->param), cmd->data_len); break;
                case SDM_REPLY_REPORT_DROP:        logger(INFO_LOG, " %s %d\n", sdm_reply_report_to_str(cmd->param), cmd->data_len); break;
                case SDM_REPLY_REPORT_SYSTIME:     logger(INFO_LOG, " %s fail\n", sdm_reply_report_to_str(cmd->param)); break;
                case SDM_REPLY_REPORT_UNKNOWN:     logger(INFO_LOG, " %s 0x%02x\n", sdm_reply_report_to_str(cmd->param), cmd->data_len); break;
                default:  logger(WARN_LOG, " Uknown reply report 0x%02x\n", cmd->param); break;
            }
            break;
        default:
            logger(WARN_LOG, "Uknown reply command 0x%02x\n", cmd->cmd);
            break;
    }

    return 0;
}

int sdm_save_samples(sdm_session_t *ss, char *buf, size_t len)
{
    int error = 0;
    int i;

    for (i = ss->streams.count - 1; i >= 0; i--) {
        int rc = stream_write(ss->streams.streams[i], (uint16_t*)buf, len / 2);

        if (rc <= 0) {
            error = rc;
            ss->streams.error_index = i;
            if (rc == STREAM_ERROR_EOS) {
                logger(INFO_LOG, "\nSink was closed: %s:%s\n",
                        stream_get_name(ss->streams.streams[i]),
                        stream_get_args(ss->streams.streams[i]));
            } else {
                logger(ERR_LOG, "\nError %s.\n", stream_strerror(ss->streams.streams[i]));
            }
            streams_remove(&ss->streams, i);
        }
    }

    return error;
}

int sdm_extract_reply(char *buf, size_t len, sdm_pkt_t **cmd)
{
    unsigned int i;
    uint64_t magic = SDM_PKG_MAGIC;
    int find = 0;

    if (len < SDM_PKT_T_SIZE) {
        *cmd = NULL;
        return 0;
    }

    for (i = 0; i < len && (len - i) >= SDM_PKT_T_SIZE; i++)
        if (!memcmp(buf + i, &magic, sizeof(magic))) {
            find = 1;
            break;
        }

    if (!find) {
        *cmd = NULL;
        return i;
    }

    *cmd = calloc(1, sizeof(sdm_pkt_t));
    sdm_unpack_reply(cmd, buf + i);
    return SDM_PKT_T_SIZE + i;
}

void sdm_set_idle_state(sdm_session_t *ss)
{
    if (ss->rx_data) {
        logger(WARN_LOG, "%s(): sdm_session->rx_data was not NULL before free."
               "Possible lost data\n" , __func__);
        free(ss->rx_data);
    }

    if (ss->rx_data_len)
        logger(WARN_LOG, "%s(): sdm_session->rx_data_len was %d."
                "Possible lost data\n", __func__, ss->rx_data_len);

    ss->rx_data = NULL;
    ss->rx_data_len = 0;

    ss->data_len = 0;
}

int sdm_buf_resize(sdm_session_t *ss, char *buf, int len)
{
    assert(ss != NULL);

    if (buf != NULL) {
        assert(len >= 0);
        if (len == 0)
            return 0;

        ss->rx_data = realloc(ss->rx_data, ss->rx_data_len + len);
        if (ss->rx_data == NULL)
            return -1;
        memcpy(ss->rx_data + ss->rx_data_len, buf, len);
        ss->rx_data_len += len;
    } else {
        if (ss->rx_data_len == -len || len >= 0) {
            if (len > 0)
                logger(WARN_LOG, "%s(): buf == NULL but len > 0: %d, "
                        "drop %d byte\n", __func__, len, ss->rx_data_len);

            free(ss->rx_data);
            ss->rx_data = NULL;
            ss->rx_data_len = 0;
            return 0;
        }

        if (len < 0) {
            len = -len;
            memmove(ss->rx_data, ss->rx_data + len, ss->rx_data_len - len);
            ss->rx_data_len -= len;
        }
    }
    return len;
}

int sdm_handle_rx_data(sdm_session_t *ss, char *buf, int len)
{
    sdm_pkt_t *cmd;
    int handled, data_len;

    if (ss == NULL)
        return 0;

    /* clear last received command */
    if (buf && len > 0) {
        sdm_buf_resize(ss, buf, len);
    }
    handled = sdm_extract_reply(ss->rx_data, ss->rx_data_len, &cmd);
    data_len = handled - SDM_PKT_T_SIZE;
    /* if we have not 16bit aligned data, we will skip last byte for this time */
    handled -= (handled % 2);
    if(handled == 0)
        return 0;

    if (cmd == NULL) {
        int rc = sdm_save_samples(ss, ss->rx_data, handled);
        if (rc < 0) {
            logger(INFO_LOG, "\n%d samples was dropped.\n", handled);
            if (rc == STREAM_ERROR_EOS)
                return SDM_ERR_SAVE_EOF;
            return SDM_ERR_SAVE_FAIL;
        }

        ss->data_len += handled;
        sdm_buf_resize(ss, NULL, -handled);
        return handled;
    }

    if (ss->state == SDM_STATE_RX && data_len != 0) {
        int rc = sdm_save_samples(ss, ss->rx_data, data_len);
        if (rc < 0) {
            logger(INFO_LOG, "\n%d samples was dropped.\n", handled);
            if (rc == STREAM_ERROR_EOS)
                return SDM_ERR_SAVE_EOF;
            return SDM_ERR_SAVE_FAIL;
        }
        ss->data_len += data_len;
    }

    /* store in sdm_session structure last received package */
    /* memcpy(&ss->cmd, cmd, sizeof(sdm_pkt_t) + cmd->data_len * 2); */
    if (ss->cmd != NULL)
        free(ss->cmd);
    ss->cmd = cmd;

    sdm_buf_resize(ss, NULL, -handled);
    sdm_show(ss, ss->cmd);

    switch (ss->cmd->cmd) {
        case SDM_REPLY_STOP:
            if (ss->streams.count && ss->data_len == 0) {
                logger(INFO_LOG, "\nReceiving STOP with data length 0. Ignore it\n");
                return handled;
            }

            if (ss->streams.count) {
                logger(INFO_LOG, "\nReceiving %d samples is done.\n", ss->data_len / 2);
                streams_clean(&ss->streams);
                sdm_set_idle_state(ss);
            }
            ss->state = SDM_STATE_IDLE;
            return handled;

        case SDM_REPLY_RX:
        case SDM_REPLY_RX_JANUS:
        case SDM_REPLY_USBL_RX:
            ss->state = SDM_STATE_RX;
            return handled;

        case SDM_REPLY_SYSTIME:
            /* cmd->data_len in header in uint16 count */
            if (handled - data_len < (int)ss->cmd->data_len * 2) {
                logger(INFO_LOG, "\rwaiting %d bytes\r", ss->cmd->data_len * 2 - handled - data_len);
                return 0;
            }

            sdm_buf_resize(ss, NULL, -ss->cmd->data_len * 2);
            handled += ss->cmd->data_len * 2;

            sdm_set_idle_state(ss);
            ss->state = SDM_STATE_IDLE;

            return handled;

        case SDM_REPLY_JANUS_DETECTED:
            /* cmd->data_len in header in uint16 count */
            if (handled - data_len < (int)ss->cmd->data_len * 2) {
                logger(INFO_LOG, "\rwaiting %d bytes\r", ss->cmd->data_len * 2 - handled - data_len);
                return 0;
            }

            sdm_handle_janus_detect(ss);

            sdm_buf_resize(ss, NULL, -ss->cmd->data_len * 2);
            handled += ss->cmd->data_len * 2;

            return handled;

        case SDM_REPLY_BUSY:
            return SDM_ERR_BUSY;
        case SDM_REPLY_SYNCIN:
            if (ss->state == SDM_STATE_WAIT_SYNCIN)
                ss->state = SDM_STATE_IDLE;
            return handled;
        case SDM_REPLY_REPORT:
            ss->state = SDM_STATE_IDLE;

            /* handle replays what can be interpreted as errors */
            switch (ss->cmd->param) {
                case SDM_REPLY_REPORT_TX_STOP:
                    /* TODO: check "sent" == "reported" number of sample */
                    break;
                case SDM_REPLY_REPORT_RX_STOP:
                    /* TODO: check "received" >= "reported" number of sample */
                    break;
                case SDM_REPLY_REPORT_NO_SDM_MODE:
                    return SDM_ERR_NO_SDM_MODE;
                case SDM_REPLY_REPORT_SYSTIME:
                case SDM_REPLY_REPORT_DROP:
                case SDM_REPLY_REPORT_UNKNOWN:
                    return -1;
                case SDM_REPLY_REPORT_REF:
                case SDM_REPLY_REPORT_CONFIG:
                case SDM_REPLY_REPORT_USBL_CONFIG:
                    if (ss->cmd->data_len == 0)
                        return -1;
            }
            return handled;
        default:
            ss->state = SDM_STATE_IDLE;
            return handled;
    }
    assert(!"must not be reached");
    return -1;
}

static int sdm_expect_v(sdm_session_t *ssl[],  struct timeval *hard_timeout, int cmd, va_list ap)
{
    int len = 0;
    char buf[BUFSIZE];
    struct timeval tv, *ptv;
    struct timeval time_limit_start = {0};

    if (hard_timeout != NULL) {
        if (gettimeofday(&time_limit_start, NULL)) {
            logger(ERR_LOG, "expect: %s\n", strerror(errno));
            return -1;
        }
    }

    logger(INFO_LOG, "expect(%s)\n", sdm_reply_to_str(cmd));
    for (;;) {
        int rc, i;
        static fd_set rfds;
        static int maxfd = 0;
        sdm_session_t *ss;

        FD_ZERO(&rfds);

        ptv = &tv;
        tv.tv_usec = 0;
        for (i = 0; ssl[i]; i++) {
            ss = ssl[i];
            FD_SET(ss->sockfd, &rfds);

            if (maxfd < ss->sockfd)
                maxfd = ss->sockfd;

            if (ss->state == SDM_STATE_INIT) {
                tv.tv_sec  = 0;
                tv.tv_usec = 10000;
                break;
            } else if (ss->timeout != -1) {
                if (ss->timeout * 1000 < tv.tv_usec) {
                    tv.tv_sec  = 0;
                    tv.tv_usec = ss->timeout * 1000;
                }
            }
        }

        if (hard_timeout != NULL) {
            struct timeval now, delta;
            if (gettimeofday(&now, NULL)) {
                logger(ERR_LOG, "expect: %s\n", strerror(errno));
                return -1;
            }
            timersub(&now, &time_limit_start, &delta);
            
            if (!timercmp(&delta, hard_timeout, <=))
                return SDM_ERR_TIMEOUT;

            if (!timercmp(&delta, hard_timeout, >))
                memcpy(&tv, hard_timeout, sizeof(tv));

        } else if (tv.tv_usec == 0)
            ptv = NULL;

        rc = select(maxfd + 1, &rfds, NULL, NULL, ptv);

        if (rc == -1) {
            logger(ERR_LOG, "expect: %s\n", strerror(errno));
            return -1;
        }

        if (hard_timeout != NULL) {
            struct timeval now, delta;
            if (gettimeofday(&now, NULL)) {
                logger(ERR_LOG, "expect: %s\n", strerror(errno));
                return -1;
            }
            timersub(&now, &time_limit_start, &delta);
            
            if (!timercmp(&delta, hard_timeout, <=))
                return SDM_ERR_TIMEOUT;
        }

        for (i = 0; ssl[i]; i++) {
            ss = ssl[i];
            /* timeout */
            if (!rc) {
                if (ss->state == SDM_STATE_INIT) {
                    ss->state = SDM_STATE_IDLE;
                    return 0;
                }

                if (ss->timeout != -1 && hard_timeout == NULL)
                    return SDM_ERR_TIMEOUT;

                goto expect_v_loop_continue;
            }

            if (FD_ISSET(ss->sockfd, &rfds)) {
                int state = ss->state;
                int len_orig;
                len_orig = len = read(ss->sockfd, buf, sizeof(buf));

                if (len == 0)
                    goto expect_v_loop_break;

                if (len < 0) {
                    logger(ERR_LOG, "expect(): %s\n", strerror(errno));
                    return -1;
                }

                do {
                    rc = sdm_handle_rx_data(ss, buf, len);

                    if (len && (cmd == SDM_REPLY_SYNCIN || (ss->cmd && !sdm_is_async_reply(ss->cmd->cmd))))
                        if (ss->rx_data_len == 0 || rc == 0) {

                            if (ss->cmd->cmd == cmd) {
                                if (cmd == SDM_REPLY_REPORT) {
                                    int rr;
                                    rr    = va_arg(ap, int);
                                    if (ss->cmd->param == rr) {
                                        switch (rr) {
                                            case SDM_REPLY_REPORT_NO_SDM_MODE: return 0;
                                            case SDM_REPLY_REPORT_TX_STOP:     return 0;
                                            case SDM_REPLY_REPORT_RX_STOP:     return 0;
                                            case SDM_REPLY_REPORT_REF:         return va_arg(ap, unsigned int) == ss->cmd->data_len;
                                            case SDM_REPLY_REPORT_CONFIG:      return va_arg(ap, unsigned int) == ss->cmd->data_len;
                                            case SDM_REPLY_REPORT_USBL_CONFIG: return va_arg(ap, unsigned int) == ss->cmd->data_len;
                                            case SDM_REPLY_REPORT_USBL_RX_STOP:return 0;
                                            case SDM_REPLY_REPORT_DROP:        return 0;
                                            case SDM_REPLY_REPORT_SYSTIME:     return 0;
                                            case SDM_REPLY_REPORT_UNKNOWN:     return 0;
                                            default:                            return 1;
                                        }
                                    }
                                }
                                if (cmd == SDM_REPLY_STOP) {
                                    return 0;
                                }
                                if (cmd == SDM_REPLY_RX) {
                                    return 0;
                                }
                                if (cmd == SDM_REPLY_RX_JANUS) {
                                    return 0;
                                }
                                if (cmd == SDM_REPLY_USBL_RX) {
                                    return 0;
                                }
                                if (cmd == SDM_REPLY_SYNCIN) {
                                    return 0;
                                }
                                if (cmd == SDM_REPLY_BUSY) {
                                    return 0;
                                }
                                return 1;
                            }
                        }
                    len = 0;
                } while (rc > 0);

                if (rc < 0) {
                    if (rc == SDM_ERR_SAVE_FAIL || rc == SDM_ERR_SAVE_EOF)
                        sdm_send(ss, SDM_CMD_STOP);
                }

                if (state == SDM_STATE_INIT) {
                    logger(WARN_LOG, "Skip %d received bytes in SDM_STATE_INIT state\n", len_orig);
                    ss->state = SDM_STATE_INIT;
                    goto expect_v_loop_continue;
                }

            }
        }
expect_v_loop_continue:;
    }
expect_v_loop_break:;
    return 0;
}

int sdm_expect(sdm_session_t *ss, int cmd, ...)
{
    va_list ap;
    int rc;
    sdm_session_t *ssl[] = {ss, NULL};

    va_start(ap, cmd);
    rc = sdm_expect_v(ssl, NULL, cmd, ap);
    va_end(ap);

    return rc;
}

int sdm_receive_data_time_limit(sdm_session_t *ssl[], long time_limit)
{
    va_list ap;
    int rc, i;
    struct timeval tv = { .tv_usec = time_limit * 1000 };
    sdm_session_t *ss;

    rc = sdm_expect_v(ssl, &tv, -1, ap);
    for (i = 0; ssl[i]; i++) {
        ss = ssl[i];
        sdm_send(ss, SDM_CMD_STOP);
    }

    return rc;
}


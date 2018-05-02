#include <stdio.h>
#include <unistd.h>     /* write() */
#include <err.h>        /* err() */
#include <sys/types.h>  /* socket() */
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>  /* inet_addr() */
#include <string.h>     /* memset() */
#include <stdarg.h>
#include <errno.h>
#include <stdlib.h>     /* stdlib() */
#include <assert.h>
#include <limits.h>    /* SHORT_MAX  */

#include <readline/readline.h>

#include <sdm.h>

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

sdm_session_t* sdm_connect(char *ip, int port)
{
    struct sockaddr_in serveraddr;
    int sockfd;

    sdm_session_t* ss = malloc(sizeof(sdm_session_t));
    if (ss == NULL)
        return NULL;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        return NULL;

    memset((char *)&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_addr.s_addr = inet_addr(ip);
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(port);

    if (connect(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
        return NULL;

    ss->sockfd = sockfd;
    ss->rx_data = NULL;
    ss->rx_data_len = 0;
    ss->state = SDM_STATE_INIT;
    ss->stream = NULL;
    ss->data_len = 0;

    return ss;
}

void sdm_close(sdm_session_t *ss)
{
    close(ss->sockfd);
    if (ss->stream)
        sdm_stream_free(ss->stream);
    if (ss->rx_data)
        free(ss->rx_data);
    free(ss);
}

int sdm_cmd(sdm_session_t *ss, int cmd_code, ...)
{
    va_list ap;
    int n;
    sdm_pkt_t *cmd = malloc(sizeof(sdm_pkt_t));
    char *data;
    int data_len = 0;

    memset(cmd, 0, sizeof(sdm_pkt_t));
    cmd->magic = SDM_PKG_MAGIC;
    cmd->cmd = cmd_code;

    switch (cmd_code) {
        case SDM_CMD_STOP:
            break;
        case SDM_CMD_CONFIG:
            va_start(ap, cmd_code);
            cmd->threshold =       va_arg(ap, int);
            cmd->gain_and_srclvl = va_arg(ap, int) << 8;
            cmd->gain_and_srclvl |= va_arg(ap, int);
            va_end(ap);
            break;
        case SDM_CMD_USBL_CONFIG:
        {
            uint32_t delay, samples;
            uint32_t gain, sample_rate;
            uint32_t tmp;
            
            va_start(ap, cmd_code);
            delay = va_arg(ap, int);
            samples = va_arg(ap, int);
            gain = va_arg(ap, int);
            sample_rate = va_arg(ap, int);
            va_end(ap);
            
            tmp = (gain << 4) + (sample_rate << 1);

            memcpy(cmd->rx_len, &tmp, 3);
            cmd->data_len = 4;
            cmd = realloc(cmd, sizeof(sdm_pkt_t) + cmd->data_len * 4);

            memcpy(cmd->data, &delay, 4);
            memcpy(&cmd->data[2], &samples, 4);
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
        case SDM_CMD_REF:
        {
            char *d;

            va_start(ap, cmd_code);

            cmd->data_len = va_arg(ap, unsigned);
            d             = va_arg(ap, char *);
            data_len = va_arg(ap, int);

            cmd = realloc(cmd, sizeof(sdm_pkt_t) + data_len * 2);
            memcpy(cmd->data, d, data_len * 2);

            va_end(ap);
            break;
        }
        case SDM_CMD_RX:
        {
            uint32_t tmp;
            va_start(ap, cmd_code);

            /* max RX length 24 bit */
            tmp = va_arg(ap, int) & 0xffffff;
            memcpy(cmd->rx_len, &tmp, 3);

            va_end(ap);
            break;
        }
        case SDM_CMD_USBL_RX:
        {
            uint8_t channel;
            uint16_t samples;
            uint32_t tmp;
            
            va_start(ap, cmd_code);
            channel = va_arg(ap, int);
            samples = va_arg(ap, int);
            va_end(ap);

            tmp = samples + (channel << 21);
            memcpy(cmd->rx_len, &tmp, 3);
            break;
        }
        default:
            free(cmd);
            return -1;
    }

    if (cmd_code == SDM_CMD_TX_CONTINUE) {
        logger(INFO_LOG, "tx cmd continue: %d samples\n", data_len);
        n = write(ss->sockfd, data, data_len * 2);
    } else {
        logger(INFO_LOG, "tx cmd %-6s: %d samples ", sdm_cmd_to_str(cmd->cmd), data_len);
        DUMP_SHORT(DEBUG_LOG, LGREEN, (uint8_t *)cmd, sizeof(sdm_pkt_t) + data_len * 2);
        logger(INFO_LOG, "\n");
        n = write(ss->sockfd, cmd, sizeof(sdm_pkt_t) + data_len * 2);
    }
    free(cmd);

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
        case SDM_CMD_REF:         return "REF";
        case SDM_CMD_CONFIG:      return "CONFIG";
        case SDM_CMD_USBL_CONFIG: return "USBL_CONFIG";
        case SDM_CMD_USBL_RX:     return "USBL_RX";
        default: return "???";
    }
}

char* sdm_reply_to_str(uint8_t cmd)
{
    switch (cmd) {
        case SDM_REPLAY_STOP:    return "STOP";
        case SDM_REPLAY_RX:      return "RX";
        case SDM_REPLAY_USBL_RX: return "USBL_RX";
        case SDM_REPLAY_BUSY:    return "BUSY";
        case SDM_REPLAY_REPORT:  return "REPORT";
        default: return "???";
    }
}

char* sdm_reply_report_to_str(uint8_t cmd)
{
    switch (cmd) {
        case SDM_REPLAY_REPORT_NO_SDM_MODE: return "NO_SDM_MODE";
        case SDM_REPLAY_REPORT_TX_STOP:     return "TX_STOP";
        case SDM_REPLAY_REPORT_RX_STOP:     return "RX_STOP";
        case SDM_REPLAY_REPORT_REF:         return "REF";
        case SDM_REPLAY_REPORT_CONFIG:      return "CONFIG";
        case SDM_REPLAY_REPORT_USBL_CONFIG: return "USBL_CONFIG";
        case SDM_REPLAY_REPORT_USBL_RX_STOP:return "RX_STOP";
        case SDM_REPLAY_REPORT_DROP:        return "DROP";
        case SDM_REPLAY_REPORT_UNKNOWN:     return "UNKNOWN";
        default: return "???";
    }
}

int sdm_show(sdm_pkt_t *cmd)
{

    logger(INFO_LOG, "\rrx cmd %-6s: ", sdm_reply_to_str(cmd->cmd));
    DUMP_SHORT(DEBUG_LOG, YELLOW, cmd, sizeof(sdm_pkt_t));

    switch (cmd->cmd) {
        case SDM_REPLAY_RX:
        case SDM_REPLAY_USBL_RX:
            /* RX do not return data_len. No need to dump data */
        case SDM_REPLAY_STOP:
            /* spaces need to clean last message 'recv %d samples' */
            logger(INFO_LOG, "          \n");
            break;
        case SDM_REPLAY_BUSY:
            logger(INFO_LOG, "%d\n", cmd->param);
            break;
        case SDM_REPLAY_REPORT:
            switch (cmd->param) {
                case SDM_REPLAY_REPORT_NO_SDM_MODE: logger(INFO_LOG, " %s\n", sdm_reply_report_to_str(SDM_REPLAY_REPORT_NO_SDM_MODE)); break;
                case SDM_REPLAY_REPORT_TX_STOP:     logger(INFO_LOG, " %s after %d samples\n", sdm_reply_report_to_str(cmd->param), cmd->data_len); break;
                case SDM_REPLAY_REPORT_RX_STOP:     logger(INFO_LOG, " %s after %d samples\n", sdm_reply_report_to_str(cmd->param), cmd->data_len); break;
                case SDM_REPLAY_REPORT_REF:         logger(INFO_LOG, " %s %s\n", sdm_reply_report_to_str(cmd->param),    cmd->data_len ? "done":"fail"); break;
                case SDM_REPLAY_REPORT_CONFIG:      logger(INFO_LOG, " %s %s\n", sdm_reply_report_to_str(cmd->param), cmd->data_len ? "done":"fail"); break;
                case SDM_REPLAY_REPORT_USBL_CONFIG: logger(INFO_LOG, " %s %s\n", sdm_reply_report_to_str(cmd->param), cmd->data_len ? "done":"fail"); break;
                case SDM_REPLAY_REPORT_USBL_RX_STOP:logger(INFO_LOG, " %s %s\n", sdm_reply_report_to_str(cmd->param), cmd->data_len); break;
                case SDM_REPLAY_REPORT_DROP:        logger(INFO_LOG, " %s %d\n", sdm_reply_report_to_str(cmd->param), cmd->data_len); break;
                case SDM_REPLAY_REPORT_UNKNOWN:     logger(INFO_LOG, " %s 0x%02x\n", sdm_reply_report_to_str(cmd->param), cmd->data_len); break;
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
    return sdm_stream_write(ss->stream, (int16_t*)buf, len / 2);
}

int sdm_load_samples(sdm_session_t *ss, int16_t *samples, size_t len)
{
    return sdm_stream_read(ss->stream, samples, len);
}

int sdm_extract_replay(char *buf, size_t len, sdm_pkt_t **cmd)
{
    unsigned int i;
    uint64_t magic = SDM_PKG_MAGIC;
    int find = 0;

    if (len < sizeof(sdm_pkt_t)) {
        *cmd = NULL;
        return 0;
    }

    for (i = 0; i < len && (len - i) >= sizeof(sdm_pkt_t); i++)
        if (!memcmp(buf + i, &magic, sizeof(magic))) {
            find = 1;
            break;
        }

    if (!find) {
        *cmd = NULL;
        return i;
    }

    *cmd = (sdm_pkt_t*)(buf + i);
    return sizeof(sdm_pkt_t) + i;
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
    memset(&ss->cmd, 0, sizeof(sdm_pkt_t));
    if (buf && len > 0) {
        sdm_buf_resize(ss, buf, len);
    }

    handled = sdm_extract_replay(ss->rx_data, ss->rx_data_len, &cmd);
    /* if we have not 16bit aligned data, we will skip last byte for this time */
    handled -= (handled % 2);
    if(handled == 0)
        return 0;

    if (cmd == NULL) {
        sdm_save_samples(ss, ss->rx_data, handled);
        ss->data_len += handled;
        logger (INFO_LOG, "\rrecv %d samples\r",  ss->data_len / 2);
        sdm_buf_resize(ss, NULL, -handled);
        return handled;
    }

    data_len = ((char *)cmd - ss->rx_data);

    if (data_len != 0) {
        sdm_save_samples(ss, ss->rx_data, data_len);
        ss->data_len += data_len;
    }

    /* store in sdm_session structure last received package */
    memcpy(&ss->cmd, cmd, sizeof(sdm_pkt_t));

    sdm_show(&ss->cmd);
    sdm_buf_resize(ss, NULL, -handled);

    switch (ss->cmd.cmd) {
        case SDM_CMD_STOP:
            /* if (ss->filename != NULL) { */
            if (ss->stream != NULL) {
                logger(INFO_LOG, "\nReceiving %d samples is done.\n", ss->data_len / 2);
                sdm_stream_close(ss->stream);
                sdm_set_idle_state(ss);
            }
            ss->state = SDM_STATE_IDLE;
            return handled;
        case SDM_CMD_RX:
        case SDM_CMD_USBL_RX:
            ss->state = SDM_STATE_RX;
            return handled;
        default:
            ss->state = SDM_STATE_IDLE;
            return handled;
    }
    /* FIXME: never reached? */
    assert(0 && "must not be reached?");
    return len;
}

int sdm_rx(sdm_session_t *ss, int cmd, ...)
{
    int len = 0;
    char buf[BUFSIZE];

    for (;;) {
        int rc;
        static fd_set rfds;
        static struct timeval tv;
        static int maxfd;

        FD_ZERO(&rfds);
        FD_SET(ss->sockfd, &rfds);
        tv.tv_sec  = 1;
        tv.tv_usec = 0;

        maxfd = ss->sockfd;

        /* if (ss->state != SDM_STATE_WAIT_REPLY) { */
            /* return 0; */
        /* } */

        rc = select(maxfd + 1, &rfds, NULL, NULL, &tv);

        if (rc == -1)
            err(1, "select()");

        /* timeout */
        if (!rc) {
            if (ss->state == SDM_STATE_INIT)
                ss->state = SDM_STATE_IDLE;
            continue;
        }

        if (FD_ISSET(ss->sockfd, &rfds)) {
            len = read(ss->sockfd, buf, sizeof(buf));

            if (len == 0)
                break;

            if (len < 0)
                err(1, "read(): ");

             rc = sdm_handle_rx_data(ss, buf, len);
            if (ss->rx_data_len == 0 || rc == 0) {
                if (ss->cmd.cmd == cmd) {
                    if (cmd == SDM_REPLAY_REPORT) {
                        int rr;
                        va_list ap;
                        va_start(ap, cmd);
                        rr    = va_arg(ap, int);
                        if (ss->cmd.param == rr) {
                            switch (rr) {
                                case SDM_REPLAY_REPORT_NO_SDM_MODE: return 0;
                                case SDM_REPLAY_REPORT_TX_STOP:     return 0;
                                case SDM_REPLAY_REPORT_RX_STOP:     return 0;
                                case SDM_REPLAY_REPORT_REF:         return va_arg(ap, unsigned int) == ss->cmd.data_len;
                                case SDM_REPLAY_REPORT_CONFIG:      return va_arg(ap, unsigned int) == ss->cmd.data_len;
                                case SDM_REPLAY_REPORT_USBL_CONFIG: return va_arg(ap, unsigned int) == ss->cmd.data_len;
                                case SDM_REPLAY_REPORT_USBL_RX_STOP:return 0;
                                case SDM_REPLAY_REPORT_DROP:        return 0;
                                case SDM_REPLAY_REPORT_UNKNOWN:     return 0;
                                default:                            return 1;
                            }
                        }
                        va_end(ap);
                    }
                    if (cmd == SDM_REPLAY_STOP) {
                        return 0;
                    }
                    if (cmd == SDM_REPLAY_RX) {
                        return 0;
                    }
                    if (cmd == SDM_REPLAY_USBL_RX) {
                        return 0;
                    }
                    if (cmd == SDM_REPLAY_BUSY) {
                        return 0;
                    }
                    return 1;
                }
            }
        }
    }
    return 0;
}

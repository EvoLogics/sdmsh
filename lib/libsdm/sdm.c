#include <stdio.h>
#include <unistd.h>     /* write() */
#include <err.h>        /* err() */
#include <sys/types.h>  /* socket() */
#include <sys/socket.h>
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
    ss->filename = NULL;
    ss->data_len = 0;

    return ss;
}

void sdm_close(sdm_session_t *ss)
{
    close(ss->sockfd);
    if (ss->filename)
        free(ss->filename);
    if (ss->rx_data)
        free(ss->rx_data);
    free(ss);
}

int sdm_cmd(sdm_session_t *ss, int cmd_code, ...)
{
    va_list ap;
    int n;
    sdm_pkt_t *cmd = malloc(sizeof(sdm_pkt_t));

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
        case SDM_CMD_TX:
        case SDM_CMD_REF:
        {
            char *d;

            va_start(ap, cmd_code);

            d             = va_arg(ap, char *);
            cmd->data_len = va_arg(ap, int) / 2;

            cmd = realloc(cmd, sizeof(sdm_pkt_t) + cmd->data_len * 2);
            memcpy(cmd->data, d, cmd->data_len * 2);

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
        default:
            free(cmd);
            return -1;
    }

    logger(INFO_LOG, "tx cmd %-6s: ", sdm_cmd_to_str(cmd->cmd));
    DUMP_SHORT(DEBUG_LOG, LGREEN, (uint8_t *)cmd, sizeof(sdm_pkt_t) + cmd->data_len * 2);
    logger(INFO_LOG, "\n");

    n = write(ss->sockfd, cmd, sizeof(sdm_pkt_t) + cmd->data_len * 2);
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
        case SDM_CMD_STOP:   return "STOP";
        case SDM_CMD_TX:     return "TX";
        case SDM_CMD_RX:     return "RX";
        case SDM_CMD_REF:    return "REF";
        case SDM_CMD_CONFIG: return "CONFIG";
        default: return "???";
    }
}

char* sdm_reply_to_str(uint8_t cmd)
{
    switch (cmd) {
        case SDM_REPLAY_STOP:   return "STOP";
        case SDM_REPLAY_RX:     return "RX";
        case SDM_REPLAY_BUSY:   return "BUSY";
        case SDM_REPLAY_REPORT: return "REPORT";
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
        case SDM_REPLAY_REPORT_DROP:        return "DROP";
        case SDM_REPLAY_REPORT_UNKNOWN:     return "UNKNOWN";
        default: return "???";
    }
}

char* sdm_samples_file_type_to_str(uint8_t type)
{
    switch(type) {
        case SDM_FILE_TYPE_FLOAT: return "FLOAT";
        case SDM_FILE_TYPE_INT:   return "INT";
        default: return "???";
    }
}

int sdm_show(sdm_pkt_t *cmd)
{

    logger(INFO_LOG, "\rrx cmd %-6s: ", sdm_reply_to_str(cmd->cmd));
    DUMP_SHORT(DEBUG_LOG, YELLOW, cmd, sizeof(sdm_pkt_t));

    switch (cmd->cmd) {
        case SDM_REPLAY_RX:
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
                case SDM_REPLAY_REPORT_TX_STOP:     logger(INFO_LOG, " %s after %d samples\n", sdm_reply_report_to_str(SDM_REPLAY_REPORT_TX_STOP), cmd->data_len); break;
                case SDM_REPLAY_REPORT_RX_STOP:     logger(INFO_LOG, " %s after %d samples\n", sdm_reply_report_to_str(SDM_REPLAY_REPORT_RX_STOP), cmd->data_len); break;
                case SDM_REPLAY_REPORT_REF:         logger(INFO_LOG, " %s %s\n", sdm_reply_report_to_str(SDM_REPLAY_REPORT_REF),    cmd->data_len ? "done":"fail"); break;
                case SDM_REPLAY_REPORT_CONFIG:      logger(INFO_LOG, " %s %s\n", sdm_reply_report_to_str(SDM_REPLAY_REPORT_CONFIG), cmd->data_len ? "done":"fail"); break;
                case SDM_REPLAY_REPORT_DROP:        logger(INFO_LOG, " %s %d\n", sdm_reply_report_to_str(SDM_REPLAY_REPORT_DROP), cmd->data_len); break;
                case SDM_REPLAY_REPORT_UNKNOWN:     logger(INFO_LOG, " %s 0x%02x\n", sdm_reply_report_to_str(SDM_REPLAY_REPORT_UNKNOWN), cmd->data_len); break;
                default:  logger(WARN_LOG, " Uknown reply report 0x%02x\n", cmd->param); break;
            }
            break;
        default:
            logger(WARN_LOG, "Uknown reply command 0x%02x\n", cmd->cmd);
            break;
    }

    return 0;
}

/**
 * search a string for any of NOT a set of bytes
 * "reverse strpbrk()"
 */
const char* strrpbrk(const char *s, const char *accept_only)
{
    for ( ;*s ;s++)
    {
        const char *a = accept_only;
        int match = 0;
        while (*a != '\0')
            if (*a++ == *s)
                match = 1;

        if (!match)
            return s;
    }
    return NULL;
}

int sdm_autodetect_samples_file_type(FILE *fp)
{
    long pos;
    char buf[40];

    fseek(fp, 0, SEEK_SET);

    /* read first line. if it's not digit (start not from [-0-9 ]), skip it */
    if (fgets(buf, sizeof(buf), fp) == NULL) {
        fprintf (stderr, "\rReading file error: %s\n", strerror(errno));
        return 0;
    }

    /* remember position a stream, to return it to proper place later */
    pos = ftell(fp);
    if (strrpbrk(buf, "-+1234567890 \n") == NULL) {
        fseek(fp, 0, SEEK_SET);
        return SDM_FILE_TYPE_INT;
    } else if (strrpbrk(buf, "-+1234567890.eE \n") == NULL) {
        fseek(fp, 0, SEEK_SET);
        return SDM_FILE_TYPE_FLOAT;
    }

    /* check second line from file */
    if (fgets(buf, sizeof(buf), fp) == NULL) {
        fprintf (stderr, "\rReading file error: %s\n", strerror(errno));
        return 0;
    }
    fseek(fp, pos, SEEK_SET);

    if (strrpbrk(buf, "-+1234567890 \n") == NULL) {
        return SDM_FILE_TYPE_INT;
    } else if (strrpbrk(buf, "-+1234567890.eE \n") == NULL) {
        return SDM_FILE_TYPE_FLOAT;
    }
    return 0;
}

int sdm_save_samples(char *filename, char *buf, size_t len)
{
    size_t i;
    int16_t *samples = (int16_t*)buf;
    FILE *fp;

    if (filename == NULL) {
        logger(ERR_LOG, "\rNo file to store data. Skipped %d byte\r", len);
        return len;
    }

    if ((fp = fopen(filename, "a")) == NULL) {
        return -1;
    }

    for (i = 0; i < len / sizeof(*samples); i++)
        fprintf (fp, "%d\n", samples[i]);

    fclose(fp);
    return 0;
}

char *sdm_load_samples(char *filename, size_t *len)
{
    FILE *fp;
    int n;
    char buf[40];
    int type;
    uint16_t *data = NULL;
    int       data_size   = 0;
    int       data_offset = 0;

    if ((fp = fopen(filename, "r")) == NULL) {
        fprintf (stderr, "\rOpen file \"%s\" error: %s\n", filename, strerror(errno));
        return NULL;
    }

    type = sdm_autodetect_samples_file_type(fp);

    logger(INFO_LOG, "\rautodect signal file type: %s\n", sdm_samples_file_type_to_str(type));
    if (type == 0) {
        fclose(fp);
        fprintf (stderr, "Can't autodetect signal file type\n");
        return NULL;
    }

    for (n = 1; ; n++) {
        double val;

        if ((fgets(buf, sizeof(buf), fp) == NULL)) {
            break;
        }
        errno = 0;
        buf[strlen(buf) - 1] = 0;

        val = strtod(buf, NULL);
        if (errno == ERANGE || (errno != 0 && val == 0)) {
            fprintf(stderr, "Line %d: Error to convert \"%s\" to digit.", n, buf);
            goto command_ref_error;
        }

        switch (type) {
            case SDM_FILE_TYPE_FLOAT:
                if (val > 1. || val < -1.) {
                    fprintf(stderr, "Line %d: Error float data do not normalized\n", n);
                    goto command_ref_error;
                }

                ADD_TO_DATA_VAL16bit(data, data_size, data_offset, (uint16_t)(val * SHRT_MAX));
                break;
            case SDM_FILE_TYPE_INT:
                if (val <= SHRT_MIN || val >= SHRT_MAX) {
                    fprintf(stderr, "Line %d: Error int data must be 16bit\n", n);
                    goto command_ref_error;
                }

                ADD_TO_DATA_VAL16bit(data, data_size, data_offset, (uint16_t)val);
                break;
        }
    }
    fclose(fp);

    *len = data_offset * 2;
    return (char *)data;

command_ref_error:
    fclose(fp);
    return NULL;
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
    if (ss->filename)
        free(ss->filename);
    ss->filename = NULL;

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

    if (ss == NULL || buf == NULL || len == 0)
        return 0;

    /* clear last received command */
    memset(&ss->cmd, 0, sizeof(sdm_pkt_t));

    sdm_buf_resize(ss, buf, len);

    handled = sdm_extract_replay(ss->rx_data, ss->rx_data_len, &cmd);

    /* if we have not 16bit aligned data, we will skip last byte for this time */
    handled -= (handled % 2);
    if(handled == 0)
        return 0;

    if (cmd == NULL) {
        if (sdm_save_samples(ss->filename, ss->rx_data, handled) == -1) {
            fprintf (stderr, "\rOpen file \"%s\" error: %s\n", ss->filename, strerror(errno));
            DUMP2LOG(DEBUG_LOG, ss->rx_data, ss->rx_data_len - handled);
        }
        ss->data_len += handled;
        logger (INFO_LOG, "\rrecv %d samples\r",  ss->data_len / 2);
        sdm_buf_resize(ss, NULL, -handled);
        return handled;
    }

    data_len = ((char *)cmd - ss->rx_data);

    if (data_len != 0) {
        if (sdm_save_samples(ss->filename, ss->rx_data, data_len) == -1) {
            fprintf (stderr, "\rOpen file \"%s\" error: %s\n", ss->filename, strerror(errno));
            DUMP2LOG(DEBUG_LOG, ss->rx_data, ss->rx_data_len - handled);
        }
        ss->data_len += data_len;
    }

    sdm_show(cmd);
    sdm_buf_resize(ss, NULL, -handled);

    switch (cmd->cmd) {
        case SDM_CMD_STOP:
            if (ss->filename != NULL) {
                logger(INFO_LOG, "Receiving %d samples to file %s is done.\n", ss->data_len / 2, ss->filename);
                sdm_set_idle_state(ss);
            }
            ss->state = SDM_STATE_IDLE;
            return handled;
        case SDM_CMD_RX:
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


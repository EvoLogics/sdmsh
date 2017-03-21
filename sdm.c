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
#include <logger.h>

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

sdm_receive_t sdm_rcv = {
    .state = SDM_STATE_IDLE,
    .filename = NULL,
    .data_len = 0,
};

int sockfd;

int sdm_connect(char *ip, int port)
{
    struct sockaddr_in serveraddr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        err(1, "socket(): ");

    memset((char *)&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_addr.s_addr = inet_addr(ip);
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(port);

    if (connect(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) 
        err(1, "connect(): ");

    return sockfd;
}

int sdm_send_cmd(int sockfd, int cmd_code, ...)
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

    logger(INFO_LOG, "snd cmd %-6s: ", sdm_cmd_to_str(cmd->cmd));
    DUMP_SHORT(DEBUG_LOG, LGREEN, (uint8_t *)cmd, sizeof(sdm_pkt_t) + cmd->data_len * 2);
    LINE2LOG;

    n = write(sockfd, cmd, sizeof(sdm_pkt_t) + cmd->data_len * 2);
    free(cmd);

    if (n < 0) {
        warn("write(): ");
        return -1;
    }

    return 0;
}

char* sdm_cmd_to_str(uint8_t cmd)
{
    switch (cmd) {
        case SDM_CMD_STOP:   return "STOP";
        case SDM_CMD_TX:     return "TX";
        case SDM_CMD_RX:     return "RX";
        case SDM_CMD_REF:    return "RE:  ";
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

    printf("\rrcv cmd %-6s: ", sdm_reply_to_str(cmd->cmd));
    DUMP_SHORT(INFO_LOG, YELLOW, cmd, sizeof(sdm_pkt_t));
    switch (cmd->cmd) {
        case SDM_REPLAY_STOP:
            printf ("\n");
            break;
        case SDM_REPLAY_RX:
            DUMP_SHORT(INFO_LOG, YELLOW, cmd->data, cmd->data_len);
            break;
        case SDM_REPLAY_BUSY:
            printf ("%d\n", cmd->param);
            break;
        case SDM_REPLAY_REPORT:
            switch (cmd->param) {
                case 0:   printf ("no SDM MODE\n"); break;
                case 1:   printf ("transmission stop after %d samples\n", cmd->data_len); break;
                case 2:   printf ("reception stop after %d samples\n", cmd->data_len); break;
                case 3:   printf ("reference update %s\n", cmd->data_len ? "finished" : "failed"); break;
                case 4:   printf ("config %s\n", cmd->data_len ? "accepted" : "failed"); break;
                case 254: printf ("garbage dropped %d\n", cmd->data_len); break;
                case 255: printf ("unknowd command code 0x%02x\n", cmd->data_len); break;
                default:  printf ("uknown reply report 0x%02x\n", cmd->param); break;
            }
            break;
        default:
            printf ("uknown reply command 0x%02x\n", cmd->cmd);
            break;
    }
    LINE2LOG;
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

    printf ("\rautodect signal file type: %s\n", sdm_samples_file_type_to_str(type));
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

void smd_rcv_idle_state()
{
    rl_clear_message();
    if (sdm_rcv.filename)
        free(sdm_rcv.filename);
    sdm_rcv.state = SDM_STATE_IDLE;
    sdm_rcv.filename = NULL;
    sdm_rcv.data_len = 0;
}


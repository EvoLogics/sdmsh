#ifndef SDM_H
#define SDM_H

#include <sys/types.h>
#include <stdint.h>
#include <stdio.h> /* FILE* */ 

#include <utils.h>
#include <stream.h>

#define SDM_ERR_SYSTEM       -1
#define SDM_ERR_TIMEOUT      -2
#define SDM_ERR_NO_SDM_MODE -42
#define SDM_ERR_BUSY        -43
#define SDM_ERR_SAVE_FAIL   -44
#define SDM_ERR_SAVE_EOF    -45

#define BUFSIZE  (1024 * 4)

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define SDM_PKG_MAGIC 0x00000000ff7f0080UL
#else
#define SDM_PKG_MAGIC 0x80007fff00000000UL
#endif

#define SDM_DEFAULT_TIMEOUT 5000 /* ms */

enum {
    SDM_CMD_STOP        = 0,
    SDM_CMD_TX          = 1,
    SDM_CMD_RX          = 2,
    SDM_CMD_REF         = 3,
    SDM_CMD_CONFIG      = 4,
    SDM_CMD_USBL_CONFIG = 5,
    SDM_CMD_USBL_RX     = 6,
    SDM_CMD_SYSTIME     = 7,
    SDM_CMD_RX_JANUS    = 8,
    SDM_JANUS_DETECTED  = 9,

    SDM_CMD_TX_CONTINUE = 128
};

enum {
    SDM_REPLY_STOP            = 0,
    SDM_REPLY_RX              = 2,
    SDM_REPLY_USBL_RX         = 6,
    SDM_REPLY_SYSTIME         = 7,
    SDM_REPLY_RX_JANUS        = 8,
    SDM_REPLY_JANUS_DETECTED  = 9,
    SDM_REPLY_SYNCIN          = 253,
    SDM_REPLY_BUSY            = 254,
    SDM_REPLY_REPORT          = 255,
};

enum {
    SDM_REPLY_REPORT_NO_SDM_MODE  = 0,
    SDM_REPLY_REPORT_TX_STOP      = 1,
    SDM_REPLY_REPORT_RX_STOP      = 2,
    SDM_REPLY_REPORT_REF          = 3,
    SDM_REPLY_REPORT_CONFIG       = 4,
    SDM_REPLY_REPORT_USBL_CONFIG  = 5,
    SDM_REPLY_REPORT_USBL_RX_STOP = 6,
    SDM_REPLY_REPORT_SYSTIME      = 7,
    SDM_REPLY_REPORT_DROP         = 254,
    SDM_REPLY_REPORT_UNKNOWN      = 255,
};


enum {
    SDM_PKT_T_OFFSET_MAGIC      = 0,
    SDM_PKT_T_OFFSET_CMD        = 8,
    SDM_PKT_T_OFFSET_THRESHOLD  = 9,
    SDM_PKT_T_OFFSET_PARAM      = 9,
    SDM_PKT_T_OFFSET_RX_LEN     = 9,
    SDM_PKT_T_OFFSET_GAIN_LVL   = 11,
    SDM_PKT_T_OFFSET_DUMMY      = 11,
    SDM_PKT_T_OFFSET_DATA_LEN   = 12,
    SDM_PKT_T_OFFSET_DATA       = 16
};

#define SDM_PKT_T_SIZE 16

typedef struct sdm_pkt_t {
    uint64_t magic;
    uint8_t  cmd;
    union {
        struct {
            uint16_t threshold;
            uint8_t  gain_and_srclvl;
        };
        struct {
            uint16_t param;
            uint8_t  dummy;
        };
        uint32_t rx_len;
    };
    uint32_t data_len; /* in 16bit words */
    union {
        int16_t data[0];
        struct {
            uint32_t current_time;
            uint32_t tx_time;
            uint32_t rx_time;
            uint32_t syncin_time;
        };
    };

} sdm_pkt_t;

enum {
    SDM_STATE_INIT = 1,
    SDM_STATE_IDLE,
    SDM_STATE_WAIT_REPLY,
    SDM_STATE_RX,
    SDM_STATE_WAIT_SYNCIN
};

enum {
    SDM_ASCII = 1,
    SDM_BIN16
};

typedef struct {
    int  sockfd;
    char *rx_data;
    int  rx_data_len;

    int state;

    int data_len;
    struct  streams_t streams;

    char   *sink_membuf;
    size_t  sink_membuf_size;

    sdm_pkt_t *cmd; /* last received command */

    long timeout; /* used in expect() */
} sdm_session_t;

sdm_session_t* sdm_connect(char *host, int port);
void  sdm_close(sdm_session_t *ss);

int   sdm_send(sdm_session_t *sd, int cmd_code, ...);
int   sdm_extract_reply(char *buf, size_t len, sdm_pkt_t **cmd);

int sdm_expect(sdm_session_t *ss, int cmd, ...);
int sdm_receive_data_time_limit(sdm_session_t *ssl[], long time_limit);

int   sdm_handle_rx_data(sdm_session_t *ss, char *buf, int len);

void  sdm_set_idle_state(sdm_session_t *ss);

int   sdm_show(sdm_session_t *ss, sdm_pkt_t *cmd);

int   sdm_save_samples(sdm_session_t *ss, char *buf, size_t len);

char* sdm_cmd_to_str(uint8_t cmd);
char* sdm_reply_to_str(uint8_t cmd);
char* sdm_reply_report_to_str(uint8_t cmd);

int sdm_is_async_reply(uint8_t cmd);

#define SDM_CHECK_ARG_LONG(name, var, range_expr) do {            \
    long arg = var;                                               \
    if (!(range_expr)) {                                          \
        logger(ERR_LOG, name": must be in range '%s'\n"           \
                , #range_expr);                                   \
        return -1;                                                \
    }                                                             \
} while (0)

#define SDM_CHECK_STR_ARG_LONG(name, str_val, out, range_expr) do {\
    long _arg;                                                     \
    errno = 0;                                                     \
    _arg = strtol(str_val, NULL, 0);                               \
    if ((errno == ERANGE && (_arg == LONG_MAX || _arg == LONG_MIN))\
        || (errno != 0 && _arg == 0)) {                            \
        logger(ERR_LOG, name": must be a digit\n");                \
        return -1;                                                 \
    }                                                              \
    SDM_CHECK_ARG_LONG(name, _arg, range_expr);                    \
    out = _arg;                                                    \
} while (0)

#endif

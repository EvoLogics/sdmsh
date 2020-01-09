#ifndef SDM_H
#define SDM_H

#include <sys/types.h>
#include <stdint.h>
#include <stdio.h> /* FILE* */ 

#include <utils.h>
#include <stream.h>

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


typedef struct sdm_pkt_t {
    uint64_t magic;
    uint8_t  cmd;
    union {
        struct {
            uint16_t threshold;
            uint8_t  gain_and_srclvl;
        } __attribute__((packed));
        struct {
            uint16_t param;
            uint8_t  dummy;
        } __attribute__((packed));
        char rx_len[3];
    };
    uint32_t data_len; /* in 16bit words */
    uint16_t data[0];
} __attribute__((packed)) sdm_pkt_t;

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

#define SDM_STREAMS_MAX 3

typedef struct {
    int  sockfd;
    char *rx_data;
    int  rx_data_len;

    int state;

    int stream_cnt;
    int stream_idx; /* Last handled stream. For error report */
    sdm_stream_t *stream[SDM_STREAMS_MAX];
    int data_len;

    sdm_pkt_t cmd; /* last received command */
} sdm_session_t;

sdm_session_t* sdm_connect(char *ip, int port);
void  sdm_close(sdm_session_t *ss);

int   sdm_cmd(sdm_session_t *sd, int cmd_code, ...);
int   sdm_extract_reply(char *buf, size_t len, sdm_pkt_t **cmd);

int   sdm_rx(sdm_session_t *ss, int cmd, ...);
int   sdm_handle_rx_data(sdm_session_t *ss, char *buf, int len);

void  sdm_set_idle_state(sdm_session_t *ss);

int   sdm_show(sdm_session_t *ss, sdm_pkt_t *cmd);

int       sdm_save_samples(sdm_session_t *ss, char *buf, size_t len);
/* int       sdm_save_samples(sdm_session_t *ss, char *filename, char *buf, size_t len); */
int sdm_load_samples(sdm_session_t *ss, int16_t *samples, size_t len);
int sdm_free_streams(sdm_session_t *ss);

const char* strrpbrk(const char *s, const char *accept_only);

char* sdm_cmd_to_str(uint8_t cmd);
char* sdm_reply_to_str(uint8_t cmd);
char* sdm_reply_report_to_str(uint8_t cmd);

int sdm_is_async_reply(uint8_t cmd);

#endif

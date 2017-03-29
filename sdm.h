#ifndef SDM_H
#define SDM_H

#include <sys/types.h>
#include <stdint.h>
#include <stdio.h> /* FILE* */ 

#define BUFSIZE  (1024 * 4)

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define SDM_PKG_MAGIC 0x00000000ff7f0080UL
#else
#define SDM_PKG_MAGIC 0x80007fff00000000UL
#endif

enum {
    SDM_CMD_STOP    = 0,
    SDM_CMD_TX      = 1,
    SDM_CMD_RX      = 2,
    SDM_CMD_REF     = 3,
    SDM_CMD_CONFIG  = 4
};

enum {
    SDM_REPLAY_STOP   = 0,
    SDM_REPLAY_RX     = 2,
    SDM_REPLAY_BUSY   = 254,
    SDM_REPLAY_REPORT = 255,
};

enum {
    SDM_FILE_TYPE_FLOAT = 1
   ,SDM_FILE_TYPE_INT
};

typedef struct sdm_pkt_t {
    uint64_t magic;
    uint8_t  cmd;
    union {
        struct {
            uint16_t threshold;
            uint8_t  gain_and_srclvl;
        } __attribute__ ((packed));
        struct {
            uint16_t param;
            uint8_t  dummy;
        } __attribute__ ((packed));
        char rx_len[3];
    };
    uint32_t data_len; /* in 16bit words */
    uint16_t data[];
} __attribute__ ((packed)) sdm_pkt_t;

enum {
    SDM_STATE_INIT = 1,
    SDM_STATE_IDLE,
    SDM_STATE_WAIT_REPLY,
    SDM_STATE_RX
};

typedef struct sdm_receive_t {
    int state;
    char* filename;
    int data_len;
} sdm_receive_t;

extern sdm_receive_t sdm_rcv;

int   sdm_connect(char *ip, int port);
int   sdm_send_cmd(int sockfd, int cmd_code, ...);
int   sdm_extract_replay(char *buf, size_t len, sdm_pkt_t **cmd);

void  smd_rcv_idle_state();

int   sdm_show(sdm_pkt_t *cmd);

int   sdm_save_samples(char *filename, char *buf, size_t len);
char* sdm_load_samples(char *filename, size_t *len);

int   sdm_autodetect_samples_file_type(FILE *fp);
const char* strrpbrk(const char *s, const char *accept_only);

char* sdm_cmd_to_str(uint8_t cmd);
char* sdm_reply_to_str(uint8_t cmd);
char* sdm_samples_file_type_to_str(uint8_t type);

extern int sockfd;

#endif


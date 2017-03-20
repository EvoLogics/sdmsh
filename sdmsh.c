#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <err.h>
#include <math.h>
#include <limits.h> /* SHRT_MAX */
#include <endian.h>
#include <assert.h>

#include <libgen.h> /* basename() */

#include <readline/readline.h>
#include <readline/history.h>

#include "sdm.h"
#include "shell.h"
#include "logger.h"

// typedef int bool;
#define true 1
#define false 0

unsigned long log_level = FATAL_LOG | ERR_LOG | WARN_LOG | INFO_LOG | DEBUG_LOG;

char buf[BUFSIZE] = {0};

int handle_receive(char *buf, int len)
{
    sdm_pkt_t *cmd;
    int handled, data_len;

    handled = sdm_extract_replay(buf, len, &cmd);
    if(handled == 0)
        return 0;

    if (cmd == NULL) {
        if (sdm_rcv.filename == NULL) {
            logger(ERR_LOG, "skip %d byte: ", handled);
            DUMP2LOG(DEBUG_LOG, buf, len - handled);
            return handled;
        }

        if (sdm_save_samples(sdm_rcv.filename, buf, handled) == -1) {
            fprintf (stderr, "\rOpen file \"%s\" error: %s\n", sdm_rcv.filename, strerror(errno));
            DUMP2LOG(DEBUG_LOG, buf, len - handled);
        }
        sdm_rcv.data_len += handled;
        return handled;
    }

    data_len = ((char *)cmd - buf);

    if (data_len != 0) {
        if (sdm_save_samples(sdm_rcv.filename, buf, data_len) == -1) {
            fprintf (stderr, "\rOpen file \"%s\" error: %s\n", sdm_rcv.filename, strerror(errno));
            DUMP2LOG(DEBUG_LOG, buf, len - handled);
        }
        sdm_rcv.data_len += data_len;
    }

    switch (cmd->cmd) {
        case SDM_CMD_STOP:
            if (sdm_rcv.filename != NULL) {
                logger(INFO_LOG, "Receiving %d samples to file %s is done.\n", sdm_rcv.data_len / 2, sdm_rcv.filename);
                smd_rcv_idle_state();
            }
            sdm_show(cmd);
            return handled;
        case SDM_CMD_RX:
        default:
            sdm_show(cmd);
            return handled;
    }
    return len;
}

int main(int argc, char *argv[])
{
    char *progname;
    int port;
    char *ip;
    fd_set rfds;
    struct timeval tv;
    int ret;

    int len;
    int count;
    int ipFlag = false;
    int portFlag = false;
    int fileFlag = false;
    int stopFlag = false;
    char *filePath;
    /* check command line arguments */
    for( count = 1; count < argc; count += 2 )
    {
        switch (argv[count][1]) {
          case 'p': case 'P':
            printf("port : %s\n", argv[count+1]);
            port = atoi(argv[count+1]);
            portFlag = true;
            break;
          case 'i': case 'I':
            printf("ip : %s\n", argv[count+1]);
            ip = argv[count+1];
            ipFlag = !ipFlag;
            break;
          case 'm': case 'M':
          {
            char concat[0xff];
            printf("ip : 192.168.0.%s\n", argv[count+1]);
            sprintf(concat, "192.168.0.%s", argv[count+1]);
            ip = concat;
            ipFlag = !ipFlag;
            break;
          }
          case 'f': case 'F':
            printf("file : %s\n", argv[count+1]);
            filePath = argv[count+1];
            fileFlag = true;
            break;
          case 's': case 'S':
            printf("stop\n");
            stopFlag = true;
            break;
          default:
            errx(0 ,"Unknow argument\n");
            break;
        }
        // printf( "  argv[%d]   %c\n", count, argv[count][1] );
        // printf( "  argv[%d]   %s\n", temp, argv[temp] );
      }
    // if (argc != 3)
    if (!((ipFlag && portFlag && !fileFlag) || (!ipFlag && !portFlag && fileFlag)))
     errx(0 ,"usage: %s -i <IP> -p <port>\n"
             "usage: %s -n <MODEM_ID> -p <port>\n"
             "usage: %s -f <EXP_FILE>\n"
             "-s\tsend stop command after init", argv[0], argv[0], argv[0]);
    // else
    //  errx(1,"go fo it");

    progname = basename(argv[0]);
    // ip = argv[1];
    // port = atoi(argv[2]);

    sockfd = sdm_connect(ip, port);
    if (fileFlag) shell_init_from_file(progname,filePath);
    else shell_init(progname);

    if (stopFlag) sdm_send_cmd(sockfd, SDM_CMD_STOP);

    for (;;) {
        FD_ZERO(&rfds);
        FD_SET(sockfd, &rfds);
        FD_SET(0, &rfds);

        tv.tv_sec  = 1;
        tv.tv_usec = 0;
        ret = select(sockfd + 1, &rfds, NULL, NULL, &tv);

        if (ret == -1)
            err(1, "select()");
        if (!ret)
            continue;

        if (FD_ISSET(0, &rfds)) {
            rl_callback_read_char();
            if(!shell_handle())
                break;
        }

        if (FD_ISSET(sockfd, &rfds)) {
            int rc;
            static int stashed = 0;
            len = read(sockfd, buf + stashed, BUFSIZE - stashed);

            if (len == 0)
                break;

            if (len < 0)
              err(1, "read(): ");

            rc = handle_receive(buf, stashed + len) - len;
            stashed = rc == -1 ? 0 : stashed - rc;
            rl_forced_update_display();
        }
    }

    shell_deinit();

    close(sockfd);

    return 0;
}

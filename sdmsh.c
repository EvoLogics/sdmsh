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

#include <sdm.h>
#include <shell.h>
#include <logger.h>

unsigned long log_level = FATAL_LOG | ERR_LOG | WARN_LOG | INFO_LOG | DEBUG_LOG;

char buf[BUFSIZE] = {0};

int handle_receive(char *buf, int len)
{
    sdm_pkt_t *cmd;
    int handled, data_len;

    handled = sdm_extract_replay(buf, len, &cmd);

    /* if we have not 16bit aligned data, we will skip last byte for this time */
    handled -= (handled % 2);
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
        logger (INFO_LOG, "\rrecv %d samples\r",  sdm_rcv.data_len / 2);
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

    /* check command line arguments */
    if (argc != 3)
       errx(0 ,"usage: %s <IP> <port>\n", argv[0]);

    progname = basename(argv[0]);
    ip = argv[1];
    port = atoi(argv[2]);

    sockfd = sdm_connect(ip, port);

    shell_init(progname);

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

            rc = handle_receive(buf, stashed + len);
            stashed = rc == -1 ? 0 : stashed + len - rc;
            if (stashed) {
                memmove(buf, &buf[rc], stashed);
            }
            if (stashed == 0)
                rl_forced_update_display();
        }
    }

    shell_deinit();

    close(sockfd);

    return 0;
}

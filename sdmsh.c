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
#include <getopt.h>

#include <libgen.h> /* basename() */

#include <readline/readline.h>
#include <readline/history.h>

#include <sdm.h>
#include <shell.h>
#include <logger.h>

#define SDM_PORT    4200

enum {
    FLAG_SHOW_HELP   = 0x01,
    FLAG_SEND_STOP   = 0x02,
    FLAG_EXEC_SCRIPT = 0x04,
};

unsigned long log_level = FATAL_LOG | ERR_LOG | WARN_LOG | INFO_LOG;

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

    sdm_show(cmd);
    switch (cmd->cmd) {
        case SDM_CMD_STOP:
            if (sdm_rcv.filename != NULL) {
                logger(INFO_LOG, "Receiving %d samples to file %s is done.\n", sdm_rcv.data_len / 2, sdm_rcv.filename);
                smd_rcv_idle_state();
            }
            sdm_rcv.state = SDM_STATE_IDLE;
            return handled;
        case SDM_CMD_RX:
            sdm_rcv.state = SDM_STATE_RX;
            return handled;
        default:
            sdm_rcv.state = SDM_STATE_IDLE;
            return handled;
    }
    return len;
}

void show_usage_and_die(char *progname) {
    printf("Usage: %s [OPTIONS] IP/NUM\n"
           "Mandatory argument IP of EvoLogics S2C Software Defined Modem. Or NUM is 192.168.0.NUM.\n"
           "\n"
           "  -f, --file=FILENAME        Run commands from FILENAME\n"
           "  -h, --help                 Display this help and exit\n"
           "  -p, --port=PORT            Set TCP PORT for connecting the SDM modem. Default is %d\n"
           "  -s, --stop                 Send SMD STOP at start\n"
           "  -v, --verbose[=log-level]  Set log level. Without parameter enable debug logging\n"
           "\n"
           "Examples:\n"
           "\n"
           "# Connect to 192.168.0.127 port 4200. Enable debug logging\n"
           "%s 127 -v\n"

           "# Connect to 10.0.0.10 to port 4201. Send SDM 'STOP' at start\n"
           "%s -sp 4201 10.0.0.10\n"

           "# Connect to 131 port 4200 and run commands from file 'rcv.sdmsh'\n"
           "%s 131 -f rcv.sdmsh\n"
           "\n"
               , progname, SDM_PORT, progname, progname, progname);
    exit(0);
}

struct option long_options[] = {
    {"file",      required_argument, 0, 'f'},
    {"help",      no_argument,       0, 'h'},
    {"port",      required_argument, 0, 'p'},
    {"stop",      no_argument,       0, 's'},
    {"verbose",   optional_argument, 0, 'v'},
};
int option_index = 0;

int main(int argc, char *argv[])
{
    char *progname, *host;
    int ret;
    int len;
    char prompt[10];

    int port = SDM_PORT;
    int opt, flags = 0;
    char *script_file = NULL;
    FILE *input = stdin;

    progname = basename(argv[0]);

    /* check command line arguments */
    while ((opt = getopt_long(argc, argv, "hv::sf:p:", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'h': flags |= FLAG_SHOW_HELP;   break;
            case 's': flags |= FLAG_SEND_STOP;   break;
            case 'p':
                      if (optarg == NULL)
                          show_usage_and_die(progname);
                      port = atoi(optarg);
                      break;

            case 'f':
                      if (optarg == NULL)
                          show_usage_and_die(progname);

                      /* POSIX standard: read script from stdin if file name '-' */
                      if (optarg[0] == '-' && optarg[1] == 0)
                          break;

                      flags |= FLAG_EXEC_SCRIPT;
                      script_file = optarg;
                      break;

            case 'v':
                      if (optarg == NULL) {
                          log_level |= DEBUG_LOG;
                          break;
                      }

                      log_level = strtoul(optarg, NULL, 0);
                      if ((errno == ERANGE && (log_level == 0 || log_level == ULONG_MAX))
                              || (errno != 0 && log_level == 0)) {
                          fprintf (stderr, "log-level: must be a digit\n");
                          return 0;
                      }

                      break;
            case '?':
                      exit (1);
            default:
                      show_usage_and_die(progname);
        }
    }

    /* As argument we need at least IP/NUM */
    if (optind >= argc)
        show_usage_and_die(progname);

    /* Mandatory argument IP/NUM */
    if (optind++ < argc) {
        static char host_[14];
        char *endptr;
        long num = strtol(argv[optind - 1], &endptr, 10);

        if (endptr != NULL && *endptr == 0 && num >= 1 && num <= 254) {
            sprintf (host_, "192.168.0.%lu", num);
            host = host_;
        } else {
            host = argv[optind - 1];
        }
    }

    if (optind < argc)
        show_usage_and_die(progname);

    if (flags & FLAG_SHOW_HELP)
        show_usage_and_die(progname);

    logger(DEBUG_LOG, "Connect to %s:%d\n", host, port);
    sockfd = sdm_connect(host, port);

    if (flags & FLAG_SEND_STOP)
        sdm_send_cmd(sockfd, SDM_CMD_STOP);

    if (flags & FLAG_EXEC_SCRIPT) {
        input = fopen(script_file, "r");
        if (input == NULL)
            err(1, "Error open script file \"%s\": ", script_file);
        logger (DEBUG_LOG, "Running script file \"%s\".\n", script_file);
    } else if (!isatty(STDIN_FILENO)) {
        flags |= FLAG_EXEC_SCRIPT;
    }

    sprintf(prompt, "%s> ", strrchr(host, '.') + 1);
    shell_init(progname, input, prompt);

    for (;;) {
        static fd_set rfds;
        static struct timeval tv;
        static int maxfd;

        FD_ZERO(&rfds);
        FD_SET(sockfd, &rfds);
        tv.tv_sec  = 1;
        tv.tv_usec = 0;

        if (flags & FLAG_EXEC_SCRIPT && feof(input) && sdm_rcv.state == SDM_STATE_IDLE)
            break;

        if (sdm_rcv.state == SDM_STATE_INIT) {
            /* In init state we want flush all data what left in modem from last session.
             * So we did't read user input till hit timeout.
             */
            tv.tv_sec  = 0;
            tv.tv_usec = 10;
            maxfd = sockfd;
        } else if (flags & FLAG_EXEC_SCRIPT && sdm_rcv.state == SDM_STATE_WAIT_REPLY) {
            /* If we running script, we need to wait for reply before run next command */
            maxfd = sockfd;
        } else {
            FD_SET(fileno(input), &rfds);
            maxfd = (sockfd > fileno(input)) ? sockfd : fileno(input);
        }

        ret = select(maxfd + 1, &rfds, NULL, NULL, &tv);

        if (ret == -1)
            err(1, "select()");
        /* timeout */
        if (!ret) {
            if (sdm_rcv.state == SDM_STATE_INIT)
                sdm_rcv.state = SDM_STATE_IDLE;
            continue;
        }

        if (FD_ISSET(fileno(input), &rfds)) {
            rl_callback_read_char();
            if(!shell_handle()) {
                /* shell want to quit */
                if (flags & FLAG_EXEC_SCRIPT) {
                    if (sdm_rcv.state == SDM_STATE_IDLE)
                        break;
                } else {
                    /* In interactive mode we want to force quit */
                    break;
                }
            }
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

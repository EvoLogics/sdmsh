/* for asprintf() */
#define _GNU_SOURCE

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

#include <sdm.h>
#include <shell.h>
#include <sdmsh_commands.h>

#define SDM_PORT    4200

enum {
    FLAG_SHOW_HELP     = 0x01,
    FLAG_SEND_STOP     = 0x02,
    FLAG_EXEC_SCRIPT   = 0x04,
    FLAG_IGNORE_ERRORS = 0x08,
};

struct shell_config shell_config;

void show_usage_and_die(int err, char *progname) {
    printf("Usage: %s [OPTIONS] IP/NUM [command; [command;] ...]\n"
           "Mandatory argument IP of EvoLogics S2C Software Defined Modem. Or NUM is 192.168.0.NUM.\n"
           "\n"
           "  -f, --file=FILENAME        Run commands from FILENAME\n"
           "  -x, --ignore-errors        If commands running from FILE, do not exit on error reply of SDM modem, \n"
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

           "# Connect to 131 port 4200 and run commands from file 'rx.sdmsh'\n"
           "%s 131 -f rx.sdmsh\n"

            "# Run commands from command line\n"
           "%s 127 'config 350 0 3; ref examples/1834_polychirp_re_down.dat; rx 2048 rcv'\n"
           "\n"
               , progname, SDM_PORT, progname, progname, progname, progname);
    exit(err);
}

struct option long_options[] = {
    {"file",          required_argument, 0, 'f'},
    {"help",          no_argument,       0, 'h'},
    {"port",          required_argument, 0, 'p'},
    {"stop",          no_argument,       0, 's'},
    {"verbose",       optional_argument, 0, 'v'},
    {"ignore-errors", no_argument,       0, 'x'},
};
int option_index = 0;

int main(int argc, char *argv[])
{
    char *progname, *host;
    int rc = 0;
    int len;
    char buf[BUFSIZE];
    sdm_session_t *sdm_session;

    int port = SDM_PORT;
    int opt, flags = 0;
    char *script_file = NULL;
    FILE *input = stdin;

    progname = basename(argv[0]);

    /* check command line arguments */
    while ((opt = getopt_long(argc, argv, "hv::sf:p:x", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'h': flags |= FLAG_SHOW_HELP;   break;
            case 's': flags |= FLAG_SEND_STOP;   break;
            case 'x': flags |= FLAG_IGNORE_ERRORS;  break;
            case 'p':
                      if (optarg == NULL)
                          show_usage_and_die(2, progname);
                      port = atoi(optarg);
                      break;

            case 'f':
                      if (optarg == NULL)
                          show_usage_and_die(2, progname);

                      /* POSIX standard: read script from stdin if file name '-' */
                      if (optarg[0] == '-' && optarg[1] == 0)
                          break;

                      flags |= FLAG_EXEC_SCRIPT;
                      script_file = optarg;
                      break;

            case 'v': {
                      char *endptr;

                      if (optarg == NULL) {
                          log_level |= DEBUG_LOG;
                          break;
                      }

                      log_level = strtoul(optarg, &endptr, 0);
                      if ((errno == ERANGE && log_level == ULONG_MAX)
                              || (errno != 0 && log_level == 0)
                              || (errno == 0 && *endptr != 0)
                              || (optarg == endptr)) {
                          fprintf (stderr, "log-level: must be a digit\n");
                          return 1;
                      }

                      break;
            }
            case '?':
                      return 2;
            default:
                      show_usage_and_die(2, progname);
        }
    }

    /* As argument we need at least IP/NUM */
    if (optind >= argc)
        show_usage_and_die(2, progname);

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

    if (flags & FLAG_SHOW_HELP)
        show_usage_and_die(0, progname);

    logger(DEBUG_LOG, "Connect to %s:%d\n", host, port);
    sdm_session = sdm_connect(host, port);

    if (sdm_session == NULL)
        err(1, "sdm_connect(\"%s:%d\"): ", host, port);

    if (flags & FLAG_SEND_STOP)
        sdm_cmd(sdm_session, SDM_CMD_STOP);

    if (optind != argc && argv[optind]) {
        if (flags & FLAG_EXEC_SCRIPT)
            errx(1, "option -f and commands from parameter can't be used in same time\n");

        shell_init_input_argv(&shell_config, argc - optind, argv + optind);
        flags |= FLAG_EXEC_SCRIPT;
        if ((input = fopen("/dev/zero", "r")) == NULL)
            err(1, "Error open file /dev/zero");
    } else if (flags & FLAG_EXEC_SCRIPT) {
        if ((input = fopen(script_file, "r")) == NULL)
            err(1, "Error open script file \"%s\": ", script_file);
        logger (DEBUG_LOG, "Running script file \"%s\".\n", script_file);
    } else if (!isatty(STDIN_FILENO)) {
        flags |= FLAG_EXEC_SCRIPT;
    }

    if (flags & FLAG_EXEC_SCRIPT)
        shell_config.prompt = NULL;
    else
        asprintf(&shell_config.prompt, "%s> ", strrchr(host, '.') + 1);

    shell_config.progname = progname;
    shell_config.input    = input;
    shell_config.cookie   = sdm_session;
    shell_config.commands = commands;
    shell_init(&shell_config);

    for (;;) {
        fd_set rfds;
        int maxfd = sdm_session->sockfd + 1;
        struct timeval tv;
        tv.tv_sec  = 0;
        tv.tv_usec = 100000;
        FD_ZERO(&rfds);
        FD_SET(sdm_session->sockfd, &rfds);
        rc = select(maxfd + 1, &rfds, NULL, NULL, &tv);
        if (rc > 0) {
            len = read(sdm_session->sockfd, buf, sizeof(buf));
            if (len > 0) continue;
        }
        break;
    }
    
    for (;;) {
        static fd_set rfds;
        static struct timeval tv;
        static int maxfd;

        FD_ZERO(&rfds);
        FD_SET(sdm_session->sockfd, &rfds);
        tv.tv_sec  = 1;
        tv.tv_usec = 0;

        if (flags & FLAG_EXEC_SCRIPT && feof(input) && sdm_session->state == SDM_STATE_IDLE)
            break;

        if (sdm_session->state == SDM_STATE_INIT) {
            /* In init state we want flush all data what left in modem from last session.
             * So we did't read user input till hit timeout.
             */
            tv.tv_sec  = 0;
            tv.tv_usec = 10;
            maxfd = sdm_session->sockfd;
        } else if (flags & FLAG_EXEC_SCRIPT &&
                   (sdm_session->state == SDM_STATE_WAIT_REPLY ||
                    sdm_session->state == SDM_STATE_RX)) {
            /* If we running script, we need to wait for reply before run next command */
            maxfd = sdm_session->sockfd;
        } else {
            FD_SET(fileno(input), &rfds);
            maxfd = (sdm_session->sockfd > fileno(input)) ? sdm_session->sockfd : fileno(input);
        }

        rc = select(maxfd + 1, &rfds, NULL, NULL, &tv);

        if (rc == -1)
            err(1, "select()");
        /* timeout */
        if (!rc) {
            if (sdm_session->state == SDM_STATE_INIT)
                sdm_session->state = SDM_STATE_IDLE;
            continue;
        }

        if (FD_ISSET(fileno(input), &rfds)) {
            rl_callback_read_char();
            if((rc = shell_handle(&shell_config)) < 0) {
                /* shell want to quit */
                rc = rc == SHELL_EOF ? 0 : rc;
                if (flags & FLAG_EXEC_SCRIPT) {
                    if (sdm_session->state == SDM_STATE_IDLE)
                        break;
                } else {
                    /* In interactive mode we want to force quit */
                    break;
                }
            }
        }

        if (FD_ISSET(sdm_session->sockfd, &rfds)) {
            len = read(sdm_session->sockfd, buf, sizeof(buf));

            if (len == 0)
                break;

            if (len < 0)
              err(1, "read(): ");

            do {
                rc = sdm_handle_rx_data(sdm_session, buf, len);
                if (len && (sdm_session->rx_data_len == 0 || rc == 0))
                    rl_forced_update_display();
                len = 0;
            } while (rc > 0);

            if (rc < 0)
                if (flags & FLAG_EXEC_SCRIPT && !(flags & FLAG_IGNORE_ERRORS))
                    break;
        }
    }

    shell_deinit(&shell_config);
    sdm_close(sdm_session);

    return rc < 0 ? -rc : rc;
}

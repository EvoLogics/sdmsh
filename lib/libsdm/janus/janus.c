#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <sdm.h>
#include <stream.h>
#include "janus.h"

int sdm_janus_rx_check_executable()
{
    char *exe;
    int rc;
    char *exe_full = strdup(JANUS_RX_CMD_FMT);

    exe = strtok(exe_full, " ");
    rc = access(exe, F_OK | X_OK);
    if (rc != 0) {
        fprintf(stderr, "janus: %s error \"%s\": ", exe, strerror(errno));
    }

    free(exe_full);
    return rc;
}

void sdm_handle_janus_detect(sdm_session_t *ss)
{
    int32_t janus_nshift;
    float janus_doppler;
    char *janus_cmd;
    sdm_stream_t *stream;

    memcpy(&janus_nshift, &ss->rx_data[0], 4);
    memcpy(&janus_doppler, &ss->rx_data[4], 4);

    /* setenv("JANUS_DETECT_NSHIFT", janus_nshift, 1); */
    /* setenv("JANUS_DETECT_DOPPLER", janus_doppler, 1); */

    asprintf(&janus_cmd, JANUS_RX_CMD_FMT, janus_nshift, janus_doppler);

    stream = sdm_stream_new(STREAM_OUTPUT, "popen", janus_cmd);
    if (stream == NULL) {
        logger(ERR_LOG, "janus: Stream creation error\n");
        sdm_cmd(ss, SDM_CMD_STOP);
        sdm_set_idle_state(ss);
    } else {
        if (sdm_stream_open(stream)) {
            logger(ERR_LOG, "janus: %s error %s\n", sdm_stream_strerror(stream));
            sdm_cmd(ss, SDM_CMD_STOP);
            sdm_set_idle_state(ss);
        } else {
            ss->stream[ss->stream_cnt] = stream;
            ss->stream_cnt++;
        }
    }
    free(janus_cmd);
}


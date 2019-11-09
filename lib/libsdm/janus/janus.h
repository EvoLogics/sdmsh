#ifndef JANUS_H
#define JANUS_H

#include <sdm.h>

/*
$ cat ./janus-rx.sh
#!/bin/sh
cd ../janus-c-3.0.5
./janus-rx $@
*/
#define JANUS_RX_CMD_FMT "./janus-rx.sh \
 --pset-file etc/parameter_sets.csv \
 --pset-id 1 \
 --stream-driver raw \
 --stream-driver-args /dev/stdin \
 --stream-fs 62500 \
 --stream-format S16 \
 --skip-detection 1 \
 --rx-once 1 \
 --verbose 9 \
 --detected-offset %d \
 --detected-doppler %f"

void sdm_handle_janus_detect(sdm_session_t *ss);

int sdm_janus_rx_check_executable();

#endif


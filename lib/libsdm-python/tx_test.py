#!/usr/bin/python

import sys
from sdm import *

session = sdm_connect("192.168.0." + sys.argv[1], 4200)

sdm_cmd(session, SDM_CMD_STOP);
sdm_rx(session, SDM_REPLAY_STOP);

sdm_cmd(session, SDM_CMD_CONFIG, 350, 0, 3);
sdm_rx(session, SDM_REPLAY_REPORT, SDM_REPLAY_REPORT_CONFIG, 1);

data  = sdm_load_samples("../../examples/1834_polychirp_re_down.dat");
data += sdm_load_samples("../../examples/1834_polychirp_re_down.dat");
sdm_cmd_tx(session, data);
sdm_rx(session, SDM_REPLAY_REPORT, SDM_REPLAY_REPORT_TX_STOP);

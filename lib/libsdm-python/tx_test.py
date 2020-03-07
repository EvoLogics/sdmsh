#!/usr/bin/python

import sys
from sdm import *

session = sdm_connect("192.168.0." + sys.argv[1], 4200)

sdm_cmd(session, SDM_CMD_STOP);
sdm_expect(session, SDM_REPLY_STOP);

sdm_cmd(session, SDM_CMD_CONFIG, 350, 0, 3, 1);
sdm_expect(session, SDM_REPLY_REPORT, SDM_REPLY_REPORT_CONFIG, 1);

data  = stream_load_samples("../../examples/0717-up.dat");
data += stream_load_samples("../../examples/0717-up.dat");

sdm_cmd_tx(session, data);
sdm_expect(session, SDM_REPLY_REPORT, SDM_REPLY_REPORT_TX_STOP);

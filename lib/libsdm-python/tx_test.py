#!/usr/bin/python

import sys
from sdm import *

#smd_rcv_idle_state()
session = sdm_connect("192.168.0." + sys.argv[1], 4200)

sdm_cmd(session, SDM_CMD_STOP);

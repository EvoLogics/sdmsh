#!/usr/bin/python

import sys
import sdm

session = sdm.connect("192.168.0." + sys.argv[1], 4200)

sdm.send(session, sdm.SDM_CMD_STOP);
sdm.expect(session, sdm.SDM_REPLY_STOP);

sdm.send_config(session, 350, 0, 3, 1);
sdm.expect(session, sdm.SDM_REPLY_REPORT, sdm.SDM_REPLY_REPORT_CONFIG, 1);
sdm.send_usbl_config(session, 0, 51200, 3, 5);
sdm.expect(session, sdm.SDM_REPLY_REPORT, sdm.SDM_REPLY_REPORT_USBL_CONFIG);

data  = sdm.stream_load_samples("../../examples/0717-up.dat");
data += sdm.stream_load_samples("../../examples/0717-up.dat");

sdm.send_tx(session, data);
sdm.expect(session, sdm.SDM_REPLY_REPORT, sdm.SDM_REPLY_REPORT_TX_STOP);
#############################################################
data = sdm.stream_load_samples("../../examples/0717-up.dat");

sdm.send_ref(session, data);
sdm.expect(session, sdm.SDM_REPLY_REPORT, sdm.SDM_REPLY_REPORT_REF);

sdm.add_sink(session, "rcv2");
sdm.send_rx(session, 1024)

sdm.expect(session, sdm.SDM_REPLY_STOP);

sdm.send(session, sdm.SDM_CMD_SYSTIME);
sdm.expect(session, sdm.SDM_REPLY_SYSTIME);

print("current_time", session.cmd.current_time);
print("tx_time",      session.cmd.tx_time);
print("rx_time",      session.cmd.rx_time);
print("syncin_time",  session.cmd.syncin_time);

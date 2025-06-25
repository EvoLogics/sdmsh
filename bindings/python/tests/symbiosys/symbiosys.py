#!/usr/bin/python3
import os
import sys
import time
import sdm

sdm.var.log_level  = sdm.FATAL_LOG | sdm.ERR_LOG | sdm.WARN_LOG
sdm.var.log_level |= sdm.NOTE_LOG
#sdm.var.log_level |= sdm.DEBUG_LOG | sdm.ASYNC_LOG

signal_file = "../../../../../../examples/0717-1ms-up.dat"
sync_number_to_handle = 5

config = {'samples': 25600,
          'threshold': 350,
          'gain': 0,
          'src_lvl': 3,
          'pream_gain': 0,
          'usbl_pream_gain': 13}

#########################################################################
def session_config(session):
    sdm.send_config(session, config['threshold'], \
                             config['gain'],      \
                             config['src_lvl'],   \
                             config['pream_gain'])
    sdm.expect(session, sdm.REPLY_REPORT, sdm.REPLY_REPORT_CONFIG)

    sdm.send_usbl_config(session, 0, config['samples'],\
                                     config['usbl_pream_gain'], 4)
    sdm.expect(session, sdm.REPLY_REPORT, sdm.REPLY_REPORT_USBL_CONFIG)

def waitsync_setup(session):

    # listen noise, threshold == 0
    sdm.send_config(session, 0, \
                             config['gain'],      \
                             config['src_lvl'],   \
                             config['pream_gain'])
    sdm.expect(session, sdm.REPLY_REPORT, sdm.REPLY_REPORT_CONFIG)

    sdm.send_usbl_config(session, 0, config['samples'],\
                                     config['usbl_pream_gain'], 4)

    sdm.expect(session, sdm.REPLY_REPORT, sdm.REPLY_REPORT_USBL_CONFIG);

#####################################################################
# workaround to set gain control to zero
# if it setup modem has to switch to receive state once
def workaround_set_gain_control_to_zero(sessions):
    for ss in sessions:
        sdm.add_sink_membuf(ss);
        sdm.send_rx(ss, 0)

    sdm.logger(sdm.NOTE_LOG, "====== workaround: reading 2 sec ======\n")
    # handle incoming data for a microsecond time
    sdm.receive_data_time_limit(sessions, 2000)

    for ss in sessions:
        sdm.expect(ss, sdm.REPLY_STOP);

#########################################################################
if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("%s: IP-active IP-passive" % sys.argv[0])
        exit(1)

    active_hosts  = [sys.argv[1]]
    passive_hosts = sys.argv[2:]

    hosts = active_hosts + passive_hosts

    sessions    = []
    for host in hosts:
        ss = sdm.create_session(host, host)
        session_config(ss)
        sessions.append(ss)
    active   = sessions[0]
    passives = sessions[1:]

    workaround_set_gain_control_to_zero(sessions)

    for i in range(sync_number_to_handle):
        for ss in sessions:
            waitsync_setup(ss)

        sdm.logger(sdm.NOTE_LOG, "%s: ====== waiting waitsynin ======\n" % active.name)
        sdm.waitsyncin(active)
        sdm.logger(sdm.NOTE_LOG, "%s: !!!!!! event   waitsynin !!!!!!\n" % active.name)

        log_dir_base = "signals/" + time.strftime("%Y%m%d-%H%M%S/")

        if not os.path.exists(log_dir_base):
            os.makedirs(log_dir_base)

        for ss in passives:
            sdm.logger(sdm.NOTE_LOG, "%s: ====== setup rx for passive =====\n" % ss.name)
            sdm.add_sink(ss, log_dir_base + "rcv-" + ss.name + ".raw");
            sdm.send_rx(ss, config['samples'])

        sdm.logger(sdm.NOTE_LOG, "%s: ====== send signal from active ======\n" % active.name)
        sdm.send_signal_file(active, signal_file, signal_file)

        sdm.logger(sdm.NOTE_LOG, "%s: ====== setup rx for active =====\n" % active.name)
        sdm.add_sink(active, log_dir_base + "rcv-" + active.name + ".raw");
        sdm.send_rx(active, config['samples'])

        for ss in passives:
            sdm.logger(sdm.NOTE_LOG, "%s: ====== receive on passive ======\n" % ss.name)
            sdm.wait_data_receive(ss);

        sdm.logger(sdm.NOTE_LOG, "%s: ====== receive on active ======\n" % active.name)
        sdm.wait_data_receive(active);

        for ss in sessions:
            sdm.logger(sdm.NOTE_LOG, "%s: ====== get USBL data ==========\n" % ss.name)
            sdm.receive_usbl_data(ss, config['samples'], log_dir_base + "u%d-" + ss.name + ".raw");
            ss.time = sdm.receive_systime(ss)

        i = 0
        for ss in sessions:
            diration = 0

            # is active side
            if i == 0:
                diration = (float(ss.time.rx) - float(ss.time.tx)) / 1000.

            out = "%s;%d;%d;%d;%d;%d\n" % (ss.name,
                                           ss.time.current,
                                           ss.time.tx,
                                           ss.time.rx,
                                           ss.time.syncin,
                                           diration)

            sdm.logger(sdm.NOTE_LOG, "%s: %s" % (ss.name, out))
            f = open(log_dir_base + "systime-" + ss.name + ".txt", "w")
            f.write(out)
            f.close()
            i += 1



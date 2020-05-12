#!/usr/bin/python3
import os
import sys
import time
import sdm

sdm.var.log_level  = sdm.FATAL_LOG | sdm.ERR_LOG | sdm.WARN_LOG
#sdm.var.log_level |= sdm.NOTE_LOG | sdm.ASYNC_LOG
sdm.var.log_level |= sdm.NOTE_LOG
#
#sdm.var.log_level |= sdm.INFO_LOG | sdm.DEBUG_LOG | sdm.ASYNC_LOG

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


def run_scenario(side_type, session):
    waitsync_setup(session)
    sdm.logger(sdm.NOTE_LOG, "%s: ====== waiting waitsynin ======\n" % session.name)
    sdm.waitsyncin(session)
    sdm.logger(sdm.NOTE_LOG, "%s: !!!!!! event   waitsynin !!!!!!\n" % session.name)

    if side_type == "active":
        sdm.logger(sdm.NOTE_LOG, "%s: ====== send signal from active ======\n" % session.name)
        sdm.send_signal_file(session, signal_file, signal_file)

    sdm.logger(sdm.NOTE_LOG, "%s: ====== setup rx for %s =====\n" % (session.name, side_type))
    sdm.add_sink(session, log_dir_base + "rcv-" + session.name + ".raw");
    sdm.send_rx(session, config['samples'])

    sdm.logger(sdm.NOTE_LOG, "%s: ====== receive on %s ======\n" % (session.name, side_type))
    sdm.wait_data_receive(session);

    sdm.receive_usbl_data(session, config['samples'], log_dir_base + "u%d-" + session.name + ".raw");
    session.time = sdm.receive_systime(session)

    diration = 0
    if side_type == "active":
        diration = (float(session.time.rx) - float(session.time.tx)) / 1000.

    out = "%s;%d;%d;%d;%d;%d\n" % (session.name,
                                   session.time.current,
                                   session.time.tx,
                                   session.time.rx,
                                   session.time.syncin,
                                   diration)

    sdm.logger(sdm.NOTE_LOG, "%s: %s" % (session.name, out))
    f = open(log_dir_base + "systime-" + session.name + ".txt", "w")
    f.write(out)
    f.close()

#########################################################################
if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("%s: IP-active IP-passive [[IP-passive] ...]" % sys.argv[0])
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
        log_dir_base = "signals/" + time.strftime("%Y%m%d-%H%M%S/")

        if not os.path.exists(log_dir_base):
            os.makedirs(log_dir_base)

        # run in parallel passive sides
        for ss in passives:
            pid = os.fork()
            if pid == 0:
                run_scenario("passive", ss)
                exit(0)

            ss.pid = pid

        run_scenario("active", active)

        for ss in passives:
            sdm.logger(sdm.NOTE_LOG, "%s: ====== waiting ending %s ======\n" % (active.name, ss.name))
            os.waitpid(ss.pid, 0)



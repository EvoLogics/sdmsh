import sys

class session_data:
    pass

def create_session(name, addr):
    session = connect(addr, 4200)
    flush_connect(session)

    session.name         = name
    session.send         = session_data()
    session.send.time    = session_data()
    session.receive      = session_data()
    session.receive.time = session_data()

    return session

def send_ref_file(session, filename):
    data = stream_load_samples(filename);
    send_ref(session, data);
    expect(session, REPLY_REPORT, REPLY_REPORT_REF);

def send_signal_file(session, ref_filename, signal_filename):
    data  = stream_load_samples(ref_filename);
    data += stream_load_samples(signal_filename);

    send_tx(session, data);
    expect(session, REPLY_REPORT, REPLY_REPORT_TX_STOP);

def receive_systime(session):
    send_systime(session)
    expect(session, REPLY_SYSTIME);

    time = session_data()
    time.current = session.cmd.current_time;
    time.tx      = session.cmd.tx_time;
    time.rx      = session.cmd.rx_time;
    time.syncin  = session.cmd.syncin_time;

    return time


def receive_usbl_data(session, nsamples, filename_pattern):
    usbl_head_number = 5
    usbl_channels = [[]] * usbl_head_number

    for ch in range(usbl_head_number):
        if filename_pattern == '':
            add_sink_membuf(session);
        else:
            add_sink(session, filename_pattern % ch);

        send_usbl_rx(session, ch, nsamples)
        expect(session, REPLY_STOP);
        if filename_pattern == '':
            usbl_channels[ch] = get_membuf(session);

    return usbl_channels

def wait_data_receive(session):
    expect(session, REPLY_STOP)

    # workaround: after REPLY_RX or REPLY_USBLRX can be several REPLY_STOP
    # requestion systime will eat them out
    receive_systime(session)

def logger(log_level, msg):
    if var.log_level & log_level:
        logger_(str(msg))

####################################################
import os
import threading

class capture_console_log(object):
    def start(self):
        sys.__stdout__.flush()

        self.stdout_save = sys.__stdout__.fileno()
        self.stdout_dup = os.dup(self.stdout_save)

        pipe_read, self.pipe_write = os.pipe()
        os.dup2(self.pipe_write, self.stdout_save)
        os.close(self.pipe_write)

        # This thread reads from the read end of the pipe.
        def capture_thread(fd):
            f = os.fdopen(fd)
            while True:
                buf = f.readline()
                if not buf:
                    break
                print(str(buf), end='', flush=True)
            f.close()

        self.thread = threading.Thread(target = capture_thread, args=[pipe_read])
        self.thread.start()

    def stop(self):

        sys.stdout.flush()
        os.dup2(self.stdout_dup, self.stdout_save)
        os.close(self.stdout_dup)

        self.thread.join()

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

def logger(log_level, msg):
    if var.log_level & log_level:
        logger_(str(msg))

def wait_data_receive(session):
    expect(session, REPLY_STOP)

    # workaround: after REPLY_RX or REPLY_USBLRX can be several REPLY_STOP
    # requestion systime will eat them out
    receive_systime(session)

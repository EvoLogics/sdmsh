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


def receive_usbl_data(session):
    usbl_channels = [[]] * 4
    for ch in range(4):
        add_sink_membuf(session);
        send_usbl_rx(session, ch, 51200)
        expect(session, REPLY_STOP);
        usbl_channels[ch] = get_membuf(session);

    return usbl_channels


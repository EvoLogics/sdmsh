%module sdm

%include stdint.i

%include exception.i
%include typemaps.i

%rename("%(strip:[sdm_])s") "";
%rename("%(strip:[SDM_])s", %$isenumitem) "";
%define EXCEPTION_RET_INT {
    if (arg1 == NULL)
        SWIG_exception(SWIG_ValueError, "No connection");

    $action

    if (result == SDM_ERR_TIMEOUT) {
        SWIG_SetErrorMsg(SDM_TimeoutError, "Timeout");
        SWIG_fail;
    } else if (result < 0)
        SWIG_exception(SWIG_SystemError, logger_last_line());
}
%enddef

%exception sdm_send_tx         EXCEPTION_RET_INT
%exception sdm_send_rx         EXCEPTION_RET_INT
%exception sdm_send_config     EXCEPTION_RET_INT
%exception sdm_send_usbl_rx    EXCEPTION_RET_INT
%exception sdm_send_ref        EXCEPTION_RET_INT
%exception sdm_send_stop       EXCEPTION_RET_INT
%exception sdm_send_rx_janus   EXCEPTION_RET_INT
%exception sdm_send_systime    EXCEPTION_RET_INT
%exception sdm_waitsyncin      EXCEPTION_RET_INT
%exception sdm_flush_connect   EXCEPTION_RET_INT
%exception sdm_add_sink        EXCEPTION_RET_INT
%exception sdm_add_sink_membuf EXCEPTION_RET_INT
%exception sdm_expect          EXCEPTION_RET_INT


%exception sdm_connect {
    if (arg1 == NULL)
        SWIG_exception(SWIG_ValueError, "No address supplied");

    $action

    if (result == NULL)
        SWIG_exception(SWIG_SystemError, "No connection");
}

%typemap(in, numinputs=0, noblock=1) size_t *len {
  size_t templen;
  $1 = &templen;
}

%typemap(out) uint16_t* {
    int i;

    if (!$1)
      SWIG_exception(SWIG_ValueError, "No signal data");

#if defined(SWIGPYTHON)
    $result = PyList_New(templen);
    for (i = 0; i < templen; i++)
        PyList_SetItem($result, i, PyInt_FromLong((double)$1[i]));
#elif define(SWIGTCL)
#endif
}

%typemap(freearg) uint16_t* {
  if (!$1)
      SWIG_exception(SWIG_ValueError, "No signal data");
  free($1);
}


%typemap(in) (size_t nsamples, uint16_t *data) {
    int i;

#if defined(SWIGPYTHON)
    if (!PyList_Check($input))
        SWIG_exception(SWIG_ValueError, "Expecting a list");

    $1 = PyList_Size($input);
    $2 = (uint16_t *) malloc(($1)*sizeof(uint16_t));
    for (i = 0; i < $1; i++) {
        PyObject *s = PyList_GetItem($input,i);
        if (!PyInt_Check(s)) {
            free($2);
            SWIG_exception(SWIG_ValueError, "List items must be integers");
        }
        $2[i] = PyInt_AsLong(s);
    }
#elif define(SWIGTCL)
#endif
}

%typemap(freearg) (size_t nsamples, uint16_t *data) {
    if (!$2)
        SWIG_exception(SWIG_ValueError, "No data");
    free($2);
}

%typemap(in) (sdm_session_t *ssl[]) {
    int i;
    size_t size;

    $1 = NULL;
#if defined(SWIGPYTHON)
    if (!PyList_Check($input))
        SWIG_exception(SWIG_ValueError, "Expecting a list in $symname");

    size = PyList_Size($input);
    if (size == 0)
        SWIG_exception(SWIG_ValueError, "Expecting a not empty list");

    $1 = (sdm_session_t **) malloc((size + 1) * sizeof(sdm_session_t *));
    for (i = 0; i < size; i++) {
        void *argp = 0;
        const int res = SWIG_ConvertPtr(PyList_GetItem($input,i), &argp, $*1_descriptor, 0);
        if (!SWIG_IsOK(res)) {
            free($1);
            SWIG_exception_fail(SWIG_ArgError(res), "in method '" "$symname" "', argument " "$argnum"" of type '" "$1_type""'");
        }
        $1[i] = (sdm_session_t *)argp;
    }
    $1[size] = NULL;
#elif define(SWIGTCL)
#endif
}

%typemap(freearg) (sdm_session_t *ssl[]) {
    if (!$1)
        SWIG_exception(SWIG_ValueError, "Expecting a list in $symname");
    free($1);
}

#if defined(SWIGPYTHON)
%pythonbegin %{
# This module provides wrappers to the S2C SDM library
%}
%pythoncode "./sdmapi.py"
#endif

%{
#define SWIG_FILE_WITH_INIT
#include <sdm.h>
#include <stream.h>
#include <stdio.h> /* fopen() */
#include <utils.h> /* logger() */
#include <janus/janus.h>

#if defined(SWIGPYTHON)
static PyObject* SDM_TimeoutError;
#endif
%}

%include <sdm.h>
%include <stream.h>
%include <utils.h>

#if defined(SWIGPYTHON)
%init %{
    SDM_TimeoutError = PyErr_NewException("sdm.TimeoutError", NULL, NULL);
    Py_INCREF(SDM_TimeoutError);
    PyModule_AddObject(m, "TimeoutError", SDM_TimeoutError);
%}

%pythoncode %{
    TimeoutError = _sdm.TimeoutError
%}
#endif

%inline %{

int sdm_send_config(sdm_session_t *ss, uint16_t threshold,
                            uint8_t gain, uint8_t srclvl, uint8_t preamp_gain)
{
    if (!ss) {
        logger(ERR_LOG, "No SDM session\n");
        return -1;
    }

    SDM_CHECK_ARG_LONG("threshold",    threshold,   arg >= 0 && arg <= 4095);
    SDM_CHECK_ARG_LONG("gain",         gain,        arg >= 0 && arg <= 1);
    SDM_CHECK_ARG_LONG("source level", srclvl,      arg >= 0 && arg <= 3);
    SDM_CHECK_ARG_LONG("preamp gain",  preamp_gain, arg >= 0 && arg <= 13);
    return sdm_send(ss, SDM_CMD_CONFIG, threshold, gain, srclvl, preamp_gain);
}

int sdm_send_usbl_config(sdm_session_t *ss, uint16_t delay, long samples,
                            uint8_t gain, uint8_t sample_rate)
{
    if (!ss) {
        logger(ERR_LOG, "No SDM session\n");
        return -1;
    }

    SDM_CHECK_ARG_LONG("delay",             delay,       arg >= 0 && arg <= 65535);
    SDM_CHECK_ARG_LONG("number of samples", samples,     arg >= 1024 && arg <= 51200 && (arg % 1024) == 0);
    SDM_CHECK_ARG_LONG("gain",              gain,        arg >= 0 && arg <= 13);
    SDM_CHECK_ARG_LONG("sample_rate",       sample_rate, arg >= 0 && arg <= 6);

    return sdm_send(ss, SDM_CMD_USBL_CONFIG, delay, samples, gain, sample_rate);
}

int sdm_send_ref(sdm_session_t *ss, size_t nsamples, uint16_t *data) {
    if (!ss) {
        logger(ERR_LOG, "No SDM session\n");
        return -1;
    }

    if (!data) {
        logger(ERR_LOG, "No data\n");
        return -1;
    }

    if (nsamples != 1024) {
        logger(WARN_LOG, "Error reference signal must be 1024 samples\n");
        return -1;
    }
    return sdm_send(ss, SDM_CMD_REF, data, nsamples);
}

int sdm_send_tx(sdm_session_t *ss, size_t nsamples, uint16_t *data) {
    int rc;
    uint16_t cmd;
    size_t cnt, passed = 0, len = 1024;

    if (!ss) {
        logger(ERR_LOG, "No SDM session\n");
        return -1;
    }

    if (!data) {
        logger(ERR_LOG, "No data\n");
        return -1;
    }

    SDM_CHECK_ARG_LONG("rx: number of samples", nsamples, arg >= 0 && arg <= 16776192);

    cmd = SDM_CMD_TX;
    do {
        len = len < nsamples - passed ? len : nsamples - passed;

        cnt = len;
        data += cnt;
        if (cnt == 0) {
            cmd = SDM_CMD_STOP;
            break;
        }

        if (cnt == len && nsamples >= 1024) {
            rc = sdm_send(ss, cmd, nsamples, data, len);
            passed += len;
        } else if (cnt > 0) {
            uint16_t buf[1024] = {0};
            memcpy(buf, data, cnt);
            rc = sdm_send(ss, cmd, nsamples, buf, 1024 * ((cnt + 1023) / 1024));
            passed += 1024 * ((cnt + 1023) / 1024);
        } else {
            logger(ERR_LOG, "sdm_send(): %s\n", strerror(errno));
            rc = -1;
        }
        cmd = SDM_CMD_TX_CONTINUE;
    } while (rc == 0 && passed < nsamples);

    return rc;
}

int sdm_send_stop(sdm_session_t *ss)
{
    return sdm_send(ss, SDM_CMD_STOP);
}

int sdm_send_rx(sdm_session_t *ss, size_t nsamples)
{
    if (!ss) {
        logger(ERR_LOG, "No SDM session\n");
        return -1;
    }

    if (ss->streams.count == 0) {
        logger(ERR_LOG, "No sink\n");
        return -1;
    }

    SDM_CHECK_ARG_LONG("rx: number of samples", nsamples, arg >= 0 && arg <= 16776192);

    if (nsamples % 1024) {
        long old = nsamples;
        nsamples += 1024 - nsamples % 1024;
        logger(WARN_LOG, "Warning: signal number of samples %ld do not divisible by 1024 samples. Expand to %ld\n"
                , old, nsamples);
    }
    return sdm_send(ss, SDM_CMD_RX, nsamples);
}

int sdm_send_usbl_rx(sdm_session_t *ss, uint8_t channel, uint16_t nsamples)
{
    if (!ss) {
        logger(ERR_LOG, "No SDM session\n");
        return -1;
    }

    if (ss->streams.count == 0) {
        logger(ERR_LOG, "No sink\n");
        return -1;
    }

    SDM_CHECK_ARG_LONG("channel number",    channel, arg >= 0 && arg <= 4);
    SDM_CHECK_ARG_LONG("number of samples", nsamples, arg >= 1024 && arg <= 51200 && (arg % 1024) == 0);

    if (nsamples % 1024) {
        long old = nsamples;
        nsamples += 1024 - nsamples % 1024;
        logger(WARN_LOG, "Warning: signal number of samples %ld do not divisible by 1024 samples. Expand to %ld\n"
                , old, nsamples);
    }

    return sdm_send(ss, SDM_CMD_USBL_RX, channel, nsamples);
}

int sdm_send_rx_janus(sdm_session_t *ss, uint16_t nsamples)
{
    if (!ss) {
        logger(ERR_LOG, "No SDM session\n");
        return -1;
    }

    if (sdm_janus_rx_check_executable() != 0) {
        logger(ERR_LOG, "No janus_rx executable\n");
        return -1;
    }

    if (ss->streams.count == 0) {
        logger(ERR_LOG, "No sink\n");
        return -1;
    }

    SDM_CHECK_ARG_LONG("number of samples", nsamples, arg >= 0 && arg <= 16776192);

    if (nsamples % 1024) {
        long old = nsamples;
        nsamples += 1024 - nsamples % 1024;
        logger(WARN_LOG, "Warning: signal number of samples %ld do not divisible by 1024 samples. Expand to %ld\n"
                , old, nsamples);
    }

    return sdm_send(ss, SDM_CMD_RX_JANUS);
}

int sdm_send_systime(sdm_session_t *ss)
{
    if (!ss) {
        logger(ERR_LOG, "No SDM session\n");
        return -1;
    }

    return sdm_send(ss, SDM_CMD_SYSTIME);
}

int sdm_waitsyncin(sdm_session_t *ss)
{
    if (!ss) {
        logger(ERR_LOG, "No SDM session\n");
        return -1;
    }

    sdm_set_idle_state(ss);
    ss->state = SDM_STATE_WAIT_SYNCIN;
    return sdm_expect(ss, SDM_REPLY_SYNCIN);
}

int sdm_flush_connect(sdm_session_t *ss)
{
    int rc;

    if (!ss) {
        logger(ERR_LOG, "No SDM session\n");
        return -1;
    }

    rc = sdm_send(ss, SDM_CMD_STOP);

    if (rc < 0)
        return rc;

    ss->state = SDM_STATE_INIT;
    return sdm_expect(ss, SDM_REPLY_STOP);
}

int sdm_add_sink(sdm_session_t *ss, char *sinkname)
{
    stream_t* stream;

    if (!ss) {
        logger(ERR_LOG, "No SDM session\n");
        return -1;
    }

    stream = streams_add_new(&ss->streams, STREAM_OUTPUT, sinkname);

    if (!stream) {
        logger(ERR_LOG, "Cannot add sink\n");
        return -1;
    }

    if (stream_open(stream)) {
        if (stream_get_errno(stream) == EINTR)
            logger(WARN_LOG, "%s: opening %s was interrupted\n", __func__, sinkname);
        else
            logger(ERR_LOG, "%s: %s error %s\n", __func__, stream_strerror(stream));
        return -1;
    }
    return 0;
}

int sdm_add_sink_membuf(sdm_session_t *ss)
{
    FILE *fp;
    stream_t* stream;

    if (!ss) {
        logger(ERR_LOG, "No SDM session\n");
        return -1;
    }

    /* sink_membuf not yet freed. Can be only one sdm_add_sink_membuf() for reciving */
    if (ss->sink_membuf) {
        logger(ERR_LOG, "Cannot set membuf sink more then one time\n");
        return -1;
    }

    stream = streams_add_new(&ss->streams, STREAM_OUTPUT, "raw:membuf");

    if (!stream) {
        logger(ERR_LOG, "Cannot add sink\n");
        return -1;
    }

    fp = open_memstream(&ss->sink_membuf, &ss->sink_membuf_size);

    if (stream_openfp(stream, fp)) {
        streams_remove(&ss->streams, ss->streams.count);
        logger(ERR_LOG, "%s: %s error %s\n", __func__, stream_strerror(stream));
        return -1;
    }
    return 0;
}

uint16_t* sdm_get_membuf(sdm_session_t *ss, size_t *len)
{
    uint16_t *sink_membuf;

    if (!ss) {
        logger(ERR_LOG, "No SDM session\n");
        return NULL;
    }

    if (!ss->sink_membuf) {
        logger(ERR_LOG, "No membuf sink\n");
        return NULL;
    }

    sink_membuf  = (uint16_t *)ss->sink_membuf;
    *len         = ss->sink_membuf_size / 2;

    ss->sink_membuf   = NULL;
    ss->sink_membuf_size = 0;

    return sink_membuf;
}


%}


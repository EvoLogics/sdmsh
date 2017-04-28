%module sdm

%typemap(in,numinputs=0,noblock=1) size_t *len {
  size_t templen;
  $1 = &templen;
}

%typemap(out) uint16_t *sdm_load_samples {
    int i;
    $result = PyList_New(templen);
    for (i = 0; i < templen; i++)
        PyList_SetItem($result, i, PyInt_FromLong((double)$1[i]));
}

%typemap(freearg)  uint16_t *sdm_load_samples {
  if ($1)
      free($1);
}

%{
#include <sdm.h>
#include <stdio.h> /* fopen() */
#include <utils.h> /* logger() */
%}

%include <sdm.h>

%typemap(in) (size_t len, uint16_t *data) {
    int i;

    if (!PyList_Check($input)) {
        PyErr_SetString(PyExc_ValueError, "Expecting a list");
        return NULL;
    }
    $1 = PyList_Size($input);
    $2 = (uint16_t *) malloc(($1)*sizeof(uint16_t));
    for (i = 0; i < $1; i++) {
        PyObject *s = PyList_GetItem($input,i);
        if (!PyInt_Check(s)) {
            free($2);
            PyErr_SetString(PyExc_ValueError, "List items must be integers");
            return NULL;
        }
        $2[i] = PyInt_AsLong(s);
    }
}

%typemap(freearg) (size_t len, uint16_t *data) {
    if ($2)
        free($2);
}

%inline %{

int sdm_cmd_ref(sdm_session_t *ss, size_t len, uint16_t *data) {
    if (len != 1024) {
        logger (WARN_LOG, "Error reference signal must be 1024 samples\n");
        return -1;
    }
    return sdm_cmd(ss, SDM_CMD_REF, data, len);
}

int sdm_cmd_tx(sdm_session_t *ss, size_t len, uint16_t *data) {
    size_t rest = len % (1024);
    if (rest) {
        rest = 1024 - rest;
        logger(WARN_LOG, "Warning: signal samples number %d do not divisible by 1024 samples. Zero padding added\n", len);
        data = realloc(data, (len + rest)*2);
        memset(data + len*2, 0, rest*2);
        len += rest;
    }
    return sdm_cmd(ss, SDM_CMD_TX, data, len);
}

int sdm_cmd_rx_to_file(sdm_session_t *ss, char *filename, size_t len) {
    FILE *fp;

    /* 16776192 == 0xfffffc maximum 24 bit digit rounded to 1024 */
    if (len < 0 && len > 16776192) {
        logger (ERR_LOG, "%s(): must be in range from 0 to 16776192\n", __func__);
        return -1;
    }

    /* truncate file if exist */
    if ((fp = fopen(filename, "w")) == NULL) {
        logger(ERR_LOG, "Error cannot open %s file: %s\n", filename, strerror(errno));
        return -1;
    }
    fclose(fp);

    if (len % 1024) {
        long old = len;
        len += 1024 - len % 1024;
        logger(WARN_LOG, "Warning: signal samples number %ld do not divisible by 1024 samples."
                " Expand to %ld\n" , old, len);
    }

    if (ss->filename) {
        free(ss->filename);
    }
    ss->filename = strdup(filename);

    sdm_cmd(ss, SDM_CMD_RX, len);
}

%}


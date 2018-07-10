//*************************************************************************
// Authors: Oleksiy Kebkal                                                *
//*************************************************************************

// ISO C headers.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#include <stream.h>
#include <error.h>

struct private_data_t
{
    // Code of last error.
    int error;
    // Last error operation.
    const char* error_op;
    
    FILE* fd;
};

static int stream_open(sdm_stream_t *stream)
{
    struct private_data_t *pdata = stream->pdata;
    
    if (stream->direction == STREAM_OUTPUT) {
        pdata->fd = fopen(stream->args, "wb");
    } else {
        pdata->fd = fopen(stream->args, "rb");
    }
    if (pdata->fd == NULL)
    {
        pdata->error_op = "opening file";
        pdata->error = errno;
        return SDM_ERROR_STREAM;
    }
    
    return SDM_ERROR_NONE;
}

static int stream_close(sdm_stream_t *stream)
{
    struct private_data_t *pdata = stream->pdata;

    if (pdata->fd == NULL)
        return 0;

    fflush(pdata->fd);
    fclose(pdata->fd);
    
    return SDM_ERROR_NONE;
}

static void stream_free(sdm_stream_t *stream)
{
    free(stream->pdata);
}

static int stream_read(const sdm_stream_t *stream, int16_t* samples, unsigned sample_count)
{
    struct private_data_t *pdata = stream->pdata;
    int rv;
    if (stream->direction == STREAM_OUTPUT) {
        return SDM_ERROR_STREAM;
    }
    rv = fread(samples, 2, sample_count, pdata->fd);
    if (rv <= 0) {
        return SDM_ERROR_STREAM;
    }
    return rv;
}

static int stream_write(sdm_stream_t *stream, void* samples, unsigned sample_count)
{
    struct private_data_t *pdata = stream->pdata;
    int rv;
    
    if (stream->direction == STREAM_INPUT) {
        return SDM_ERROR_STREAM;
    }
    rv = (int)fwrite(samples, stream->sample_size, sample_count, pdata->fd);
    if (rv <= 0) {
        return SDM_ERROR_STREAM;
    }
    if ((unsigned)rv != sample_count) {
        return SDM_ERROR_STREAM;
    }
    return rv;
}

static const char* stream_get_error(sdm_stream_t *stream)
{
    struct private_data_t *pdata = stream->pdata;
    return strerror(pdata->error);
}

static const char* stream_get_error_op(sdm_stream_t *stream)
{
    struct private_data_t *pdata = stream->pdata;
    return pdata->error_op;
}

static int stream_count(sdm_stream_t* stream)
{
    if (stream->direction == STREAM_OUTPUT) {
        return 0;
    }
    struct stat st;
    stat(stream->args, &st);
    return st.st_size / 2;
}

int sdm_stream_raw_new(sdm_stream_t *stream)
{
    stream->pdata = calloc(1, sizeof(struct private_data_t));
    stream->open = stream_open;
    stream->close = stream_close;
    stream->free = stream_free;
    stream->read = stream_read;
    stream->write = stream_write;
    stream->get_error = stream_get_error;
    stream->get_error_op = stream_get_error_op;
    stream->count = stream_count;
    strcpy(stream->name, "RAW");
    
    return SDM_ERROR_NONE;
}

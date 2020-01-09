//*************************************************************************
// Authors: Oleksiy Kebkal                                                *
//*************************************************************************

// ISO C headers.
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#include <stream.h>
#include <error.h>

struct private_data_t
{
    // Code of last error.
    int error;
    // Last error operation.
    const char* error_op;
    
    int fd;
};

static int stream_open(sdm_stream_t *stream)
{
    struct private_data_t *pdata;

    if (!stream)
        return SDM_ERROR_STREAM;
    pdata = stream->pdata;
    
    if (stream->direction == STREAM_OUTPUT) {
        pdata->fd = open(stream->args, O_CREAT|O_WRONLY, 0664);
    } else {
        pdata->fd = open(stream->args, O_RDONLY);
    }

    if (pdata->fd < 0)
        RETURN_ERROR("opening file", errno);
    
    return SDM_ERROR_NONE;
}

static int stream_close(sdm_stream_t *stream)
{
    struct private_data_t *pdata;

    if (!stream)
        return SDM_ERROR_STREAM;
    pdata = stream->pdata;

    if (pdata->fd == -1)
        return 0;

    close(pdata->fd);
    pdata->fd = -1;
    
    return SDM_ERROR_NONE;
}

static void stream_free(sdm_stream_t *stream)
{
    if (stream && stream->pdata) {
        free(stream->pdata);
        stream->pdata = NULL;
    }
}

static int stream_read(const sdm_stream_t *stream, int16_t* samples, unsigned sample_count)
{
    struct private_data_t *pdata;
    int rc;

    if (!stream)
        return SDM_ERROR_STREAM;
    pdata = stream->pdata;

    if (stream->direction == STREAM_OUTPUT)
        RETURN_ERROR("reading file", ENOTSUP);

    rc = read(pdata->fd, samples, sample_count * stream->sample_size);

    if (rc < 0)
        RETURN_ERROR("reading file", errno);

    return rc / stream->sample_size;
}

static int stream_write(sdm_stream_t *stream, void* samples, unsigned sample_count)
{
    struct private_data_t *pdata;
    int rc;
    
    if (!stream)
        return SDM_ERROR_STREAM;
    pdata = stream->pdata;

    if (stream->direction == STREAM_INPUT)
        RETURN_ERROR("writing file", ENOTSUP);

    rc = write(pdata->fd, samples, stream->sample_size * sample_count);

    if (rc < 0)
        RETURN_ERROR("writing file", errno);

    if ((unsigned)rc != stream->sample_size * sample_count)
        return SDM_ERROR_STREAM;

    return rc / stream->sample_size;
}

static int stream_get_errno(sdm_stream_t *stream)
{
    struct private_data_t *pdata;

    if (!stream)
        return EINVAL;
    pdata = stream->pdata;

    return pdata->error;
}

static const char* stream_strerror(sdm_stream_t *stream)
{
    struct private_data_t *pdata;

    if (!stream)
        return "No stream";
    pdata = stream->pdata;

    return strerror(pdata->error);
}

static const char* stream_get_error_op(sdm_stream_t *stream)
{
    struct private_data_t *pdata;

    if (!stream)
        return "No stream";
    pdata = stream->pdata;

    return pdata->error_op;
}

static int stream_count(sdm_stream_t* stream)
{
    struct stat st;
    struct private_data_t *pdata;

    if (!stream)
        return SDM_ERROR_STREAM;
    pdata = stream->pdata;

    if (stream->direction == STREAM_OUTPUT)
        RETURN_ERROR("counting samples in file", ENOTSUP);

    if (stat(stream->args, &st) < 0)
        RETURN_ERROR("reading file", errno);

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
    stream->get_errno = stream_get_errno;
    stream->strerror = stream_strerror;
    stream->get_error_op = stream_get_error_op;
    stream->count = stream_count;
    strcpy(stream->name, "RAW");
    
    return SDM_ERROR_NONE;
}

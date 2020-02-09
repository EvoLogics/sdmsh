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

struct private_data_t
{
    // Code of last error.
    int error;
    // Last error operation.
    const char* error_op;

    int fd;
};

static int stream_impl_open(stream_t *stream)
{
    struct private_data_t *pdata;

    if (!stream)
        return STREAM_ERROR;
    pdata = stream->pdata;

    if (stream->direction == STREAM_OUTPUT) {
        pdata->fd = open(stream->args, O_CREAT|O_WRONLY, 0664);
    } else {
        pdata->fd = open(stream->args, O_RDONLY);
    }

    if (pdata->fd < 0)
        RETURN_ERROR("opening file", errno);

    return STREAM_ERROR_NONE;
}

static int stream_impl_close(stream_t *stream)
{
    struct private_data_t *pdata;

    if (!stream)
        return STREAM_ERROR;
    pdata = stream->pdata;

    if (pdata->fd == -1)
        return 0;

    close(pdata->fd);
    pdata->fd = -1;

    return STREAM_ERROR_NONE;
}

static void stream_impl_free(stream_t *stream)
{
    if (stream && stream->pdata) {
        free(stream->pdata);
        stream->pdata = NULL;
    }
}

static int stream_impl_read(const stream_t *stream, uint16_t* samples, unsigned sample_count)
{
    struct private_data_t *pdata;
    int rc;

    if (!stream)
        return STREAM_ERROR;
    pdata = stream->pdata;

    if (stream->direction == STREAM_OUTPUT)
        RETURN_ERROR("reading file", ENOTSUP);

    rc = read(pdata->fd, samples, sample_count * stream->sample_size);

    if (rc < 0)
        RETURN_ERROR("reading file", errno);

    return rc / stream->sample_size;
}

static int stream_impl_write(stream_t *stream, void* samples, unsigned sample_count)
{
    struct private_data_t *pdata;
    int rc;

    if (!stream)
        return STREAM_ERROR;
    pdata = stream->pdata;

    if (stream->direction == STREAM_INPUT)
        RETURN_ERROR("writing file", ENOTSUP);

    rc = write(pdata->fd, samples, stream->sample_size * sample_count);

    if (rc < 0)
        RETURN_ERROR("writing file", errno);

    if ((unsigned)rc != stream->sample_size * sample_count)
        return STREAM_ERROR;

    return rc / stream->sample_size;
}

static int stream_impl_get_errno(stream_t *stream)
{
    struct private_data_t *pdata;

    if (!stream)
        return EINVAL;
    pdata = stream->pdata;

    return pdata->error;
}

static const char* stream_impl_strerror(stream_t *stream)
{
    struct private_data_t *pdata;

    if (!stream)
        return "No stream";
    pdata = stream->pdata;

    return strerror(pdata->error);
}

static const char* stream_impl_get_error_op(stream_t *stream)
{
    struct private_data_t *pdata;

    if (!stream)
        return "No stream";
    pdata = stream->pdata;

    return pdata->error_op;
}

static int stream_impl_count(stream_t* stream)
{
    struct stat st;
    struct private_data_t *pdata;

    if (!stream)
        return STREAM_ERROR;
    pdata = stream->pdata;

    if (stream->direction == STREAM_OUTPUT)
        RETURN_ERROR("counting samples in file", ENOTSUP);

    if (stat(stream->args, &st) < 0)
        RETURN_ERROR("reading file", errno);

    return st.st_size / 2;
}

int stream_impl_raw_new(stream_t *stream)
{
    stream->pdata = calloc(1, sizeof(struct private_data_t));
    stream->open         = stream_impl_open;
    stream->close        = stream_impl_close;
    stream->free         = stream_impl_free;
    stream->read         = stream_impl_read;
    stream->write        = stream_impl_write;
    stream->get_errno    = stream_impl_get_errno;
    stream->strerror     = stream_impl_strerror;
    stream->get_error_op = stream_impl_get_error_op;
    stream->count        = stream_impl_count;
    strcpy(stream->name, "RAW");

    return STREAM_ERROR_NONE;
}

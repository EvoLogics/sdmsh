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

    FILE* fp;
};

static int stream_impl_open(stream_t *stream)
{
    struct private_data_t *pdata = stream->pdata;

    if (stream->direction == STREAM_OUTPUT) {
        pdata->fp = fopen(stream->args, "w");
    } else {
        pdata->fp = fopen(stream->args, "r");
    }

    if (!pdata->fp)
        STREAM_RETURN_ERROR("opening file", errno);

    return STREAM_ERROR_NONE;
}

static int stream_impl_openfp(stream_t *stream, FILE *fp)
{
    struct private_data_t *pdata = stream->pdata;
    pdata->fp = fp;
    return STREAM_ERROR_NONE;
}

static int stream_impl_close(stream_t *stream)
{
    struct private_data_t *pdata = stream->pdata;

    if (!pdata->fp)
        return 0;

    fclose(pdata->fp);
    pdata->fp = NULL;

    return STREAM_ERROR_NONE;
}

static void stream_impl_free(stream_t *stream)
{
    free(stream->pdata);
    stream->pdata = NULL;
}

static int stream_impl_read(const stream_t *stream, uint16_t* samples, unsigned sample_count)
{
    struct private_data_t *pdata = stream->pdata;
    int rc;


    if (stream->direction == STREAM_OUTPUT)
        STREAM_RETURN_ERROR("reading file", ENOTSUP);

    rc = fread(samples, stream->sample_size, sample_count, pdata->fp);

    if ((unsigned)rc != sample_count)
        STREAM_RETURN_FP("reading file", STREAM_ERROR_IO, pdata->fp);

    return rc;
}

static int stream_impl_write(stream_t *stream, void* samples, unsigned sample_count)
{
    struct private_data_t *pdata = stream->pdata;
    int rc;


    if (stream->direction == STREAM_INPUT)
        STREAM_RETURN_ERROR("writing file", ENOTSUP);

    rc = fwrite(samples, stream->sample_size, sample_count, pdata->fp);

    if ((unsigned)rc != sample_count)
        STREAM_RETURN_ERROR("writing file", errno);

    return rc;
}

static int stream_impl_get_errno(stream_t *stream)
{
    struct private_data_t *pdata = stream->pdata;
    return pdata->error;
}

static const char* stream_impl_strerror(stream_t *stream)
{
    struct private_data_t *pdata = stream->pdata;

    return strerror(pdata->error);
}

static const char* stream_impl_get_error_op(stream_t *stream)
{
    struct private_data_t *pdata = stream->pdata;

    return pdata->error_op;
}

static int stream_impl_count(stream_t* stream)
{
    struct private_data_t *pdata = stream->pdata;
    struct stat st;

    if (stream->direction == STREAM_OUTPUT)
        STREAM_RETURN_ERROR("counting samples in file", ENOTSUP);

    if (stat(stream->args, &st) < 0)
        STREAM_RETURN_ERROR("reading file", errno);

    return st.st_size / 2;
}

int stream_impl_raw_new(stream_t *stream)
{
    stream->pdata = calloc(1, sizeof(struct private_data_t));
    stream->open         = stream_impl_open;
    stream->openfp       = stream_impl_openfp;
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

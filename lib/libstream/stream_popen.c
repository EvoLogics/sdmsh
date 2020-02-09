/*************************************************************************
 * Authors: Maksym Komar                                                 *
 *************************************************************************
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <errno.h>

#include <stream.h>

struct private_data_t {
    int error; /* Code of last error. */
    const char* error_op; /* Last error operation. */
    FILE* fd;
};


static int stream_impl_open(stream_t *stream)
{
    struct private_data_t *pdata = stream->pdata;

    pdata->fd = popen(stream->args,
            stream->direction == STREAM_OUTPUT ? "w" : "r");

    if (pdata->fd == NULL)
        RETURN_ERROR("popen process", errno);

    return STREAM_ERROR_NONE;
}

static int stream_impl_close(stream_t *stream)
{
    struct private_data_t *pdata;

    if (!stream)
        return STREAM_ERROR;
    pdata = stream->pdata;

    if (pdata->fd == NULL)
        return STREAM_ERROR;

    if (pclose(pdata->fd) == -1)
        RETURN_ERROR("pclose process", errno);

    return STREAM_ERROR_NONE;
}

static void stream_impl_free(stream_t *stream)
{
    if (stream && stream->pdata) {
        free(stream->pdata);
        stream->pdata = NULL;
    }
}

static int stream_impl_read(const stream_t *stream, uint16_t* samples, unsigned samples_count)
{
    struct private_data_t *pdata;
    size_t rc, offset = 0;

    if (!stream)
        return STREAM_ERROR;
    pdata = stream->pdata;

    if (stream->direction == STREAM_OUTPUT)
        RETURN_ERROR("reading to write only process", ENOTSUP);

    do {
        rc = fread(samples + offset, 2, samples_count - offset, pdata->fd);
        if (rc != samples_count - offset) {
            if (ferror(pdata->fd))
                RETURN_ERROR("reading from stream", errno);

            if (feof(pdata->fd))
                return STREAM_ERROR;
        }
        offset += rc;
    } while (offset < samples_count);

    return samples_count;
}

static int stream_impl_write(stream_t *stream, void* samples, unsigned int samples_count)
{
    struct private_data_t *pdata = stream->pdata;
    size_t rc, offset = 0;

    if (!stream)
        return STREAM_ERROR;
    pdata = stream->pdata;

    if (stream->direction == STREAM_INPUT)
        RETURN_ERROR("writing to read only process", ENOTSUP);

    do {
        rc = fwrite(samples + offset, 2, samples_count - offset, pdata->fd);
        if (rc != samples_count - offset) {
            if (ferror(pdata->fd)) {
                if (errno == EPIPE) {
                    errno = 0;
                    return STREAM_ERROR_EOS;
                }
                RETURN_ERROR("writing to stream", errno);
            }

            if (feof(pdata->fd))
                return STREAM_ERROR;
        }
        offset += rc;
    } while (offset < samples_count);

    return samples_count;
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
    struct private_data_t *pdata;

    if (!stream)
        return STREAM_ERROR;
    pdata = stream->pdata;

    RETURN_ERROR("stream count", EAFNOSUPPORT);
}

int stream_impl_popen_new(stream_t *stream)
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
    strcpy(stream->name, "POPEN");

    return STREAM_ERROR_NONE;
}

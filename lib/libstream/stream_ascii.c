//*************************************************************************
// Authors: Oleksiy Kebkal                                                *
//*************************************************************************

// ISO C headers.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include <stream.h>

enum {
    STREAM_ASCII_FILE_TYPE_FLOAT = 1
   ,STREAM_ASCII_FILE_TYPE_INT
};

static const char* strrpbrk(const char *s, const char *accept_only);

struct private_data_t
{
    // Code of last error.
    int error;
    // Last error operation.
    const char* error_op;

    int file_type;

    FILE* fd;
};

int autodetect_samples_file_type(stream_t *stream)
{
    struct private_data_t *pdata;
    FILE *fp;
    long pos;
    char buf[40];

    if (!stream)
        return STREAM_ERROR;
    pdata = stream->pdata;
    fp = pdata->fd;
    pdata->file_type = 0;

    if (fp == NULL)
        return 0;

    if (fseek(fp, 0, SEEK_SET) < 0)
        RETURN_ERROR("seek in file", errno);

    /* read first line. if it's not digit (start not from [-0-9 ]), skip it */
    if (fgets(buf, sizeof(buf), fp) == NULL)
        RETURN_ERROR("reading file", errno);

    /* remember position a stream, to return it to proper place later */
    pos = ftell(fp);
    if (strrpbrk(buf, "-+1234567890 \n") == NULL) {
        fseek(fp, 0, SEEK_SET);
        pdata->file_type = STREAM_ASCII_FILE_TYPE_INT;
        return 0;
    } else if (strrpbrk(buf, "-+1234567890.eE \n") == NULL) {
        fseek(fp, 0, SEEK_SET);
        pdata->file_type = STREAM_ASCII_FILE_TYPE_FLOAT;
        return 0;
    }

    /* check second line from file */
    if (fgets(buf, sizeof(buf), fp) == NULL)
        RETURN_ERROR("reading file", errno);
    fseek(fp, pos, SEEK_SET);

    if (strrpbrk(buf, "-+1234567890 \n") == NULL) {
        pdata->file_type = STREAM_ASCII_FILE_TYPE_INT;
        return 0;
    } else if (strrpbrk(buf, "-+1234567890.eE \n") == NULL) {
        pdata->file_type = STREAM_ASCII_FILE_TYPE_FLOAT;
        return 0;
    }
    return 0;
}

static int stream_impl_open(stream_t *stream)
{
    struct private_data_t *pdata = stream->pdata;
    int rc = 0;

    if (stream->direction == STREAM_OUTPUT) {
        pdata->fd = fopen(stream->args, "w");
        pdata->file_type = STREAM_ASCII_FILE_TYPE_INT;
    } else {
        pdata->fd = fopen(stream->args, "r");
        rc = autodetect_samples_file_type(stream);
    }
    if (pdata->fd == NULL || rc < 0)
        RETURN_ERROR("opening file", errno);

    if (pdata->file_type == 0) {
        fclose(pdata->fd);
        RETURN_ERROR("Can't autodetect signal file type", errno);
    }
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

    fflush(pdata->fd);
    fclose(pdata->fd);

    return STREAM_ERROR_NONE;
}

static void stream_impl_free(stream_t *stream)
{
    if (stream && stream->pdata) {
        free(stream->pdata);
        stream->pdata = NULL;
    }
}

static int stream_impl_read(const stream_t *stream, int16_t* samples, unsigned sample_count)
{
    unsigned n;
    char buf[40];
    struct private_data_t *pdata;
    int data_offset = 0;

    if (!stream)
        return STREAM_ERROR;
    pdata = stream->pdata;

    if (stream->direction == STREAM_OUTPUT)
        RETURN_ERROR("writing file", ENOTSUP);

    for (n = 0; n < sample_count; n++) {
        double val;

        if ((fgets(buf, sizeof(buf), pdata->fd) == NULL)) {
            break;
        }
        errno = 0;
        buf[strlen(buf) - 1] = 0;
        val = strtod(buf, NULL);
        if (errno == ERANGE || (errno != 0 && val == 0)) {
            RETURN_ERROR("Error to convert to digit.", errno);
        }

        switch (pdata->file_type) {
            case STREAM_ASCII_FILE_TYPE_FLOAT:
                if (val > 1. || val < -1.)
                    RETURN_ERROR("Error float data do not normalized", ERANGE);

                samples[data_offset++] = (int16_t)(val * SHRT_MAX);
                break;
            case STREAM_ASCII_FILE_TYPE_INT:
                if (val <= SHRT_MIN || val >= SHRT_MAX)
                    RETURN_ERROR("Error int data must be 16bit", ERANGE);

                samples[data_offset++] = (int16_t)val;
                break;
        }
    }
    return n;
}

static int stream_impl_write(stream_t *stream, void* samples, unsigned int sample_count)
{
    struct private_data_t *pdata = stream->pdata;
    unsigned int i;

    if (!stream)
        return STREAM_ERROR;
    pdata = stream->pdata;

    if (stream->direction == STREAM_INPUT)
        RETURN_ERROR("writing file", ENOTSUP);

    for (i = 0; i < sample_count; i++)
        if (fprintf (pdata->fd, "%d\n", ((int16_t*)samples)[i]) < 0)
            RETURN_ERROR("writing file", errno);

    return sample_count;
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
    FILE *fp;
    char line[80];
    int lines=0;

    if (!stream)
        return STREAM_ERROR;
    pdata = stream->pdata;

    if (stream->direction == STREAM_OUTPUT)
        RETURN_ERROR("reading file", ENOTSUP);

    fp = fopen(stream->args,"r");
    if (fp == NULL)
        RETURN_ERROR("reading file", errno);

    while (fgets(line, sizeof(line), fp))
        lines++;

    fclose(fp);

    return lines;
}

int stream_impl_ascii_new(stream_t *stream)
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
    strcpy(stream->name, "ASCII");

    return STREAM_ERROR_NONE;
}

/**
 * search a string for any of NOT a set of bytes
 * "reverse strpbrk()"
 */
const char* strrpbrk(const char *s, const char *accept_only)
{
    for ( ;*s ;s++)
    {
        const char *a = accept_only;
        int match = 0;
        while (*a != '\0')
            if (*a++ == *s)
                match = 1;

        if (!match)
            return s;
    }
    return NULL;
}

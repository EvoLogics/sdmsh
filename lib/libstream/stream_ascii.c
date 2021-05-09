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

    FILE* fp;
};

int autodetect_samples_file_type(stream_t *stream)
{
    struct private_data_t *pdata = stream->pdata;
    FILE *fp;
    char buf[40];

    fp = pdata->fp;
    pdata->file_type = 0;

    if (fp == NULL)
        return 0;

    if (fseek(fp, 0, SEEK_SET) < 0)
        STREAM_RETURN_ERROR("seek in file", errno);

    for (;;) {
        /* read first line. if it's not digit (start not from [-0-9 ]), skip it */
        if (fgets(buf, sizeof(buf), fp) == NULL) {
            fseek(fp, 0, SEEK_SET);
            if (errno != 0) {
                pdata->file_type = 0;
                STREAM_RETURN_ERROR("reading file", errno);
            }
            return 0;
        }

        /* skip empty strings, comments started with # or // */
        if(buf[0] == 0 || buf[0] == '#' || (buf[0] == '/' && buf[1] == '/'))
            continue;

        /* skip 0 becouse it's can be int or float */
        if(strrpbrk(buf,"0 \n") == NULL) {
            pdata->file_type = STREAM_ASCII_FILE_TYPE_INT;
            continue;
        }

        if (strrpbrk(buf, "-+1234567890 \n") == NULL) {
            pdata->file_type = STREAM_ASCII_FILE_TYPE_INT;
            break;
        } else if (strrpbrk(buf, "-+1234567890.eE \n") == NULL
                || strrpbrk(buf, "-+1234567890. \n")   == NULL) {
            pdata->file_type = STREAM_ASCII_FILE_TYPE_FLOAT;
            break;
        }
    }
    fseek(fp, 0, SEEK_SET);
    return 0;
}

static int stream_impl_open(stream_t *stream)
{
    struct private_data_t *pdata = stream->pdata;
    int rc = 0;

    errno = 0;
    if (stream->direction == STREAM_OUTPUT) {
        pdata->fp = fopen(stream->args, "w");
        pdata->file_type = STREAM_ASCII_FILE_TYPE_INT;
    } else {
        pdata->fp = fopen(stream->args, "r");
        rc = autodetect_samples_file_type(stream);
    }

    if (pdata->fp == NULL || rc < 0)
        STREAM_RETURN_ERROR("opening file", errno);

    if (pdata->file_type == 0) {
        fclose(pdata->fp);
        STREAM_RETURN_ERROR("Can't autodetect signal file type", errno);
    }

    return STREAM_ERROR_NONE;
}

static int stream_impl_close(stream_t *stream)
{
    struct private_data_t *pdata = stream->pdata;

    if (pdata->fp == NULL)
        return STREAM_ERROR;

    fflush(pdata->fp);
    fclose(pdata->fp);

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
    unsigned n;
    char buf[40];
    int data_offset = 0;


    if (stream->direction == STREAM_OUTPUT)
        STREAM_RETURN_ERROR("writing file", ENOTSUP);

    for (n = 0; n < sample_count;) {
        double val;

        if ((fgets(buf, sizeof(buf), pdata->fp) == NULL)) {
            break;
        }

        /* skip empty strings, comments started with # or // */
        if(buf[0] == 0 || buf[0] == '#' || (buf[0] == '/' && buf[1] == '/'))
            continue;

        errno = 0;
        buf[strlen(buf) - 1] = 0;
        val = strtod(buf, NULL);
        if (errno == ERANGE || (errno != 0 && val == 0)) {
            STREAM_RETURN_ERROR("Error to convert to digit.", errno);
        }

        switch (pdata->file_type) {
            case STREAM_ASCII_FILE_TYPE_FLOAT:
                if (val > 1. || val < -1.)
                    STREAM_RETURN_ERROR("Error float data do not normalized", ERANGE);

                samples[data_offset++] = (uint16_t)(val * SHRT_MAX);
                break;
            case STREAM_ASCII_FILE_TYPE_INT:
                if (val <= SHRT_MIN || val >= SHRT_MAX)
                    STREAM_RETURN_ERROR("Error int data must be 16bit", ERANGE);

                samples[data_offset++] = (uint16_t)val;
                break;
        }
        n++;
    }
    return n;
}

static int stream_impl_write(stream_t *stream, void* samples, unsigned int sample_count)
{
    struct private_data_t *pdata = stream->pdata;
    unsigned int i;

    if (stream->direction == STREAM_INPUT)
        STREAM_RETURN_ERROR("writing file", ENOTSUP);

    for (i = 0; i < sample_count; i++)
        if (fprintf (pdata->fp, "%d\n", ((uint16_t*)samples)[i]) < 0)
            STREAM_RETURN_ERROR("writing file", errno);

    return sample_count;
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
    FILE *fp;
    char line[80];
    int lines=0;


    if (stream->direction == STREAM_OUTPUT)
        STREAM_RETURN_ERROR("reading file", ENOTSUP);

    fp = fopen(stream->args,"r");
    if (fp == NULL)
        STREAM_RETURN_ERROR("reading file", errno);

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

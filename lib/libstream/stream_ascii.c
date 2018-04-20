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
#include <error.h>

enum {
    SDM_FILE_TYPE_FLOAT = 1
   ,SDM_FILE_TYPE_INT
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

int sdm_autodetect_samples_file_type(sdm_stream_t *stream)
{
    struct private_data_t *pdata = stream->pdata;
    FILE *fp = pdata->fd;
    long pos;
    char buf[40];

    pdata->file_type = 0;
    fseek(fp, 0, SEEK_SET);

    /* read first line. if it's not digit (start not from [-0-9 ]), skip it */
    if (fgets(buf, sizeof(buf), fp) == NULL) {
        fprintf (stderr, "\rReading file error: %s\n", strerror(errno));
        return 0;
    }

    /* remember position a stream, to return it to proper place later */
    pos = ftell(fp);
    if (strrpbrk(buf, "-+1234567890 \n") == NULL) {
        fseek(fp, 0, SEEK_SET);
        pdata->file_type = SDM_FILE_TYPE_INT;
        return 0;
    } else if (strrpbrk(buf, "-+1234567890.eE \n") == NULL) {
        fseek(fp, 0, SEEK_SET);
        pdata->file_type = SDM_FILE_TYPE_FLOAT;
        return 0;
    }

    /* check second line from file */
    if (fgets(buf, sizeof(buf), fp) == NULL) {
        fprintf (stderr, "\rReading file error: %s\n", strerror(errno));
        return 0;
    }
    fseek(fp, pos, SEEK_SET);

    if (strrpbrk(buf, "-+1234567890 \n") == NULL) {
        pdata->file_type = SDM_FILE_TYPE_INT;
        return 0;
    } else if (strrpbrk(buf, "-+1234567890.eE \n") == NULL) {
        pdata->file_type = SDM_FILE_TYPE_FLOAT;
        return 0;
    }
    return 0;
}

static int stream_open(sdm_stream_t *stream)
{
    struct private_data_t *pdata = stream->pdata;

    if (stream->direction == STREAM_OUTPUT) {
        pdata->fd = fopen(stream->args, "w");
    } else {
        pdata->fd = fopen(stream->args, "r");
    }
    if (pdata->fd == NULL)
    {
        pdata->error_op = "opening file";
        pdata->error = errno;
        return SDM_ERROR_STREAM;
    }
    sdm_autodetect_samples_file_type(stream);
    if (pdata->file_type == 0) {
        fclose(pdata->fd);
        fprintf (stderr, "Can't autodetect signal file type\n");
        return SDM_ERROR_STREAM;
    }
    return SDM_ERROR_NONE;
}

static int stream_close(sdm_stream_t *stream)
{
    struct private_data_t *pdata = stream->pdata;

    fflush(pdata->fd);
    fclose(pdata->fd);
    
    return 0;
}

static void stream_free(sdm_stream_t *stream)
{
    free(stream->pdata);
}

static int stream_read(const sdm_stream_t *stream, int16_t* samples, unsigned sample_count)
{
    unsigned n;
    char buf[40];
    struct private_data_t *pdata = stream->pdata;
    FILE *fp = pdata->fd;
    int data_offset = 0;
    
    if (stream->direction == STREAM_OUTPUT) {
        return SDM_ERROR_STREAM;
    }
    for (n = 0; n < sample_count; n++) {
        double val;

        if ((fgets(buf, sizeof(buf), fp) == NULL)) {
            break;
        }
        errno = 0;
        buf[strlen(buf) - 1] = 0;
        val = strtod(buf, NULL);
        if (errno == ERANGE || (errno != 0 && val == 0)) {
            fprintf(stderr, "Line %d: Error to convert \"%s\" to digit.", n, buf);
            goto command_ref_error;
        }
        switch (pdata->file_type) {
            case SDM_FILE_TYPE_FLOAT:
                if (val > 1. || val < -1.) {
                    fprintf(stderr, "Line %d: Error float data do not normalized\n", n);
                    goto command_ref_error;
                }
                samples[data_offset++] = (int16_t)(val * SHRT_MAX);
                break;
            case SDM_FILE_TYPE_INT:
                if (val <= SHRT_MIN || val >= SHRT_MAX) {
                    fprintf(stderr, "Line %d: Error int data must be 16bit\n", n);
                    goto command_ref_error;
                }
                samples[data_offset++] = (int16_t)val;
                break;
        }
    }
    return n;
command_ref_error:
    return 0;
}

static int stream_write(sdm_stream_t *stream, void* samples, unsigned int sample_count)
{
    struct private_data_t *pdata = stream->pdata;
    unsigned int i;

    if (stream->direction == STREAM_INPUT) {
        return SDM_ERROR_STREAM;
    }
    
    for (i = 0; i < sample_count; i++)
        fprintf (pdata->fd, "%d\n", ((int16_t*)samples)[i]);
    return 0;
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
    FILE *fp;
    char line[80];
    int lines=0;

    if (stream->direction == STREAM_OUTPUT) {
        return 0;
    }
    fp = fopen(stream->args,"r");
    if (fp == NULL)
        return 0;
    while (fgets(line, sizeof(line), fp))
        lines++;
    fclose(fp);
    return lines;
}

int sdm_stream_ascii_new(sdm_stream_t *stream)
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
    strcpy(stream->name, "ASCII");
    
    return SDM_ERROR_NONE;
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

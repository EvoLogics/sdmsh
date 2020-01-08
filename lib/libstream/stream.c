//*************************************************************************
// Authors: Oleksiy Kebkal                                                *
//*************************************************************************

// ISO C headers.
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wordexp.h>

#include <stream.h>
#include <error.h>
// Declare driver initialization functions.
#define STREAM(a) \
    int sdm_stream_ ## a ## _new(sdm_stream_t*);
#include "stream.def"

static const char* s_drivers[] = {
#define STREAM(a) #a,
#include "stream.def"
    NULL
};

const char** sdm_stream_get_drivers(void)
{
    return s_drivers;
}

sdm_stream_t *sdm_stream_new(int direction, const char* driver, const char* args)
{
    sdm_stream_t *stream = (sdm_stream_t*)calloc(1, sizeof(sdm_stream_t));
    int rv = 0;
    wordexp_t wbuf;

    /* Need to expand ~ after <driver>: prefix */
    wordexp(args, &wbuf, WRDE_NOCMD);
    if (wbuf.we_wordc == 1)
        strcpy(stream->args, wbuf.we_wordv[0]);
    else
        strcpy(stream->args, args);
    wordfree(&wbuf);

    stream->sample_size = sizeof(int16_t);
    stream->direction = direction;
    sdm_stream_set_fs(stream, 62500);

// Select appropriate driver.
#define STREAM(a)                                                      \
    if (strcmp(#a, driver) == 0) {rv = sdm_stream_ ## a ## _new(stream);} else
#include "stream.def"
    {rv = SDM_ERROR_STREAM;}
  
    if (rv != SDM_ERROR_NONE) {
        free(stream);
        return NULL;
    }
    return stream;
}

void sdm_stream_free(sdm_stream_t *stream)
{
    if (stream->free)
        stream->free(stream);
    free(stream);
    stream = NULL;
}

void sdm_stream_set_fs(sdm_stream_t *stream, unsigned fs)
{
    stream->fs = fs;
}

unsigned sdm_stream_get_fs(sdm_stream_t *stream)
{
    return stream->fs;
}

unsigned sdm_stream_get_sample_size(sdm_stream_t *stream)
{
    return stream->sample_size;
}

int sdm_stream_open(sdm_stream_t *stream)
{
    return stream->open(stream);
}

int sdm_stream_close(sdm_stream_t *stream)
{
    return stream->close(stream);
}

int sdm_stream_read(sdm_stream_t *stream, int16_t* samples, unsigned sample_count)
{
    if (stream) {
        return stream->read(stream, samples, sample_count);
    } else {
        return SDM_ERROR_STREAM;
    }
}

int sdm_stream_write(sdm_stream_t *stream, int16_t *samples, unsigned sample_count)
{
    if (stream) {
        return stream->write(stream, samples, sample_count);
    } else {
        return SDM_ERROR_STREAM;
    }
}

unsigned sdm_stream_count(sdm_stream_t *stream)
{
    return stream->count(stream);
}

int sdm_stream_get_errno(sdm_stream_t *stream)
{
    return stream->get_errno(stream);
}

const char* sdm_stream_strerror(sdm_stream_t *stream)
{
    sprintf(stream->bfr_error, "%s \"%s\": %s", stream->get_error_op(stream)
            , stream->args, stream->strerror(stream));
    return stream->bfr_error;
}

const char* sdm_stream_get_name(sdm_stream_t *stream)
{
    if (!stream)
        return "No stream";
    return stream->name;
}

const char* sdm_stream_get_args(sdm_stream_t *stream)
{
    if (!stream)
        return "No stream";
    return stream->args;
}

void sdm_stream_dump(sdm_stream_t *stream)
{
    fprintf(stderr, "Stream Driver: %s", stream->name);
    fprintf(stderr, "Stream Driver Arguments: %s", stream->args);
    fprintf(stderr, "Stream Bytes Per Sample: %u", stream->sample_size);
    fprintf(stderr, "Stream Sampling Frequency (Hz): %u", stream->fs);
}

//*************************************************************************
// This is free software: you can redistribute it and/or modify it        *
// under the terms of the GNU General Public License version 3 as         *
// published by the Free Software Foundation.                             *
//                                                                        *
// This program is distributed in the hope that it will be useful, but    *
// WITHOUT ANY WARRANTY; without even the implied warranty of FITNESS     *
// FOR A PARTICULAR PURPOSE. See the GNU General Public License for       *
// more details.                                                          *
//                                                                        *
// You should have received a copy of the GNU General Public License      *
// along with this program. If not, see <http://www.gnu.org/licenses/>.   *
//*************************************************************************
// Authors: Oleksiy Kebkal                                                *
//*************************************************************************

// ISO C headers.
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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

    strcpy(stream->args, args);
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
    return stream->read(stream, samples, sample_count);
}

int sdm_stream_write(sdm_stream_t *stream, int16_t *samples, unsigned sample_count)
{
    return stream->write(stream, samples, sample_count);
}

unsigned sdm_stream_count(sdm_stream_t *stream)
{
    return stream->count(stream);
}

const char* sdm_stream_get_error(sdm_stream_t *stream)
{
    sprintf(stream->bfr_error, "%s: %s", stream->get_error_op(stream),
            stream->get_error(stream));
    return stream->bfr_error;
}

void sdm_stream_dump(sdm_stream_t *stream)
{
    fprintf(stderr, "Stream Driver: %s", stream->name);
    fprintf(stderr, "Stream Driver Arguments: %s", stream->args);
    fprintf(stderr, "Stream Bytes Per Sample: %u", stream->sample_size);
    fprintf(stderr, "Stream Sampling Frequency (Hz): %u", stream->fs);
}

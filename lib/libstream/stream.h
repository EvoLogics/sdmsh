//*************************************************************************
// Authors: Oleksiy Kebkal                                                *
//*************************************************************************

#ifndef STREAM_H_INCLUDED_
#define STREAM_H_INCLUDED_

#include <stdint.h>
#include <stdio.h>  /* FILE* */

#include <stream_error.h>

typedef struct stream_t stream_t;

struct stream_t
{
    //! Sampling frequency (Hz).
    unsigned fs;
    //! Sample size (in bytes).
    unsigned sample_size;
    
    //! Driver name.
    char name[36];
    //! Driver args.
    char args[256];
    
    //! Pointer to driver's open function.
    int (*open)(stream_t*);
    //! Pointer to driver's openfp function.
    int (*openfp)(stream_t*, FILE *);
    //! Pointer to driver's close function.
    int (*close)(stream_t*);
    //! Pointer to driver's free function.
    void (*free)(stream_t*);
    //! Pointer to driver's read function.
    int (*read)(const stream_t*, uint16_t*, unsigned);
    //! Pointer to driver's write function.
    int (*write)(stream_t*, void*, unsigned);
    //! Number of samples in stream, if known.
    int (*count)(stream_t*);
    //! Pointer to driver's error function.
    int (*get_errno)(stream_t*);
    //! Pointer to driver's error translating function.
    const char* (*strerror)(stream_t*);
    //! Pointer to driver's error translating function.
    const char* (*get_error_op)(stream_t*);
    
    //! Private data used by stream implementation.
    void* pdata;
    //! Error message buffer.
    char bfr_error[512];

    int direction;
};

enum {
    STREAM_OUTPUT = 1
    ,STREAM_INPUT
};

typedef struct streams_t streams_t;
#define STREAMS_MAX 3
struct streams_t {
    unsigned int count;
    stream_t *streams[STREAMS_MAX];
    int error_index; /* Last handled stream. For error report */
};

#define STREAM_SET_ERROR(descr, errno_code) \
    do {                                    \
        pdata->error_op = descr;            \
        pdata->error = errno_code;          \
    } while (0)

#define STREAM_RETURN_ERROR(descr, errno_code) \
    do {                                       \
        pdata->error_op = descr;               \
        pdata->error = errno_code;             \
        return STREAM_ERROR;                   \
    } while (0)

#define STREAM_RETURN_FP(descr, stream_error, _fp) \
    do {                                           \
        int ret;                                   \
        if (feof(_fp))                             \
            ret = STREAM_ERROR_EOS;                \
        if (ferror(_fp)) {                         \
            pdata->error_op = descr;               \
            pdata->error = stream_error;           \
            ret = STREAM_ERROR;                    \
        }                                          \
        clearerr(_fp);                             \
        return ret;                                \
    } while (0)

//! Retrieve the list of drivers.
//! @return list of drivers.
const char** stream_get_drivers(void);

//! Create output stream object.
//! @param direction STREAM_OUTPUT or STREAM_INPUT
//! @param description driver's description string
//! @return output stream object.
stream_t* stream_new(int direction, char *description);

//! Create output stream object.
//! @param direction STREAM_OUTPUT or STREAM_INPUT
//! @param driver driver's name.
//! @param args driver's arguments.
//! @return output stream object.
stream_t *stream_new_v(int direction, const char* driver, const char* args);

//! Free output stream.
//! @param stream output stream.
void stream_free(stream_t *stream);

//! Set sampling frequency.
//! @param stream output stream object.
//! @param fs sampling frequency (Hz).
void stream_set_fs(stream_t *stream, unsigned fs);

//! Retrieve sampling frequency.
//! @param stream output stream object.
//! @return sampling frequency (Hz).
unsigned stream_get_fs(stream_t *stream);

//! Retrieve sample size.
//! @param stream output stream object.
//! @return sample size in bytes.
unsigned stream_get_sample_size(stream_t *stream);

//! Open stream.
//! @param stream output stream object.
int stream_open(stream_t *stream);

//! Open stream by FILE*
//! @param stream output stream object
//! @fp    already opened FILE*
int stream_openfp(stream_t *stream, FILE *fp);

//! Close stream.
//! @param stream output stream object.
int stream_close(stream_t *stream);

//! Read samples from stream.
//! @param stream input stream object.
//! @param samples array of samples.
//! @param sample_count number of samples.
int stream_read(stream_t *stream, uint16_t* samples, unsigned sample_count);

//! Write samples to stream.
//! @param stream output stream object.
//! @param samples array of samples.
//! @param sample_count number of samples.
int stream_write(stream_t *stream, uint16_t *samples, unsigned sample_count);

//! Get stream samples count
//! @param stream output stream object.
ssize_t stream_count(stream_t *stream);

//! Retrieve the last error.
//! @param stream output stream object.
//! @return last error
int stream_get_errno(stream_t *stream);

//! Retrieve the last error.
//! @param stream output stream object.
//! @return last error in string form.
const char* stream_strerror(stream_t *stream);

//! Retrieve the name of stream
//! @param stream output stream object.
//! @return name of strem
const char* stream_get_name(stream_t *stream);

//! Retrieve the arguments of stream
//! @param stream output stream object.
//! @return arguments of strem
const char* stream_get_args(stream_t *stream);

//! Print stream parameters to standard output.
//! @param stream output stream object.
void stream_dump(stream_t *stream);

/****************** streams_t ***********************/
//! Create and add output stream object to streams
//! @param streams streams list to add
//! @param direction STREAM_OUTPUT or STREAM_INPUT
//! @param description driver's description
//! @return output stream object.
stream_t* streams_add_new(streams_t *streams, int direction, char *description);

//! Add stream to streams.
//! @param streams output streams object.
//! @param stream which will be added.
int streams_add(streams_t *streams, stream_t *stream);

//! Remove stream from streams.
//! @param streams output streams object.
//! @param index which will be removed
int streams_remove(streams_t *streams, unsigned int index);

//! Clean streams. Close and free memory
//! @param streams output streams object.
void streams_clean(streams_t *streams);

/*************************************************/
//! Read samples from stream.
//! @param stream intput stream object.
//! @param samples array of samples.
//! @param sample_count number of samples.
uint16_t* stream_load_samples(char *filename, size_t *len);

#endif

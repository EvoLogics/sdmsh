//*************************************************************************
// Authors: Oleksiy Kebkal                                                *
//*************************************************************************

#ifndef SDM_STREAM_H_INCLUDED_
#define SDM_STREAM_H_INCLUDED_

#include <stdint.h>

typedef struct sdm_stream_t sdm_stream_t;

struct sdm_stream_t
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
    int (*open)(sdm_stream_t*);
    //! Pointer to driver's close function.
    int (*close)(sdm_stream_t*);
    //! Pointer to driver's free function.
    void (*free)(sdm_stream_t*);
    //! Pointer to driver's read function.
    int (*read)(const sdm_stream_t*, int16_t*, unsigned);
    //! Pointer to driver's write function.
    int (*write)(sdm_stream_t*, void*, unsigned);
    //! Number of samples in stream, if known.
    int (*count)(sdm_stream_t*);
    //! Pointer to driver's error translating function.
    const char* (*get_error)(sdm_stream_t*);
    //! Pointer to driver's error translating function.
    const char* (*get_error_op)(sdm_stream_t*);
    
    //! Private data used by stream implementation.
    void* pdata;
    //! Error message buffer.
    char bfr_error[128];

    int direction;
};

enum {
    STREAM_OUTPUT = 1
    ,STREAM_INPUT
};

//! Retrieve the list of drivers.
//! @return list of drivers.
const char** sdm_stream_get_drivers(void);

//! Create output stream object.
//! @param driver driver's name.
//! @param args driver's arguments.
//! @return output stream object.
sdm_stream_t *sdm_stream_new(int direction, const char* driver, const char* args);

//! Free output stream.
//! @param stream output stream.
void sdm_stream_free(sdm_stream_t *stream);

//! Set sampling frequency.
//! @param stream output stream object.
//! @param fs sampling frequency (Hz).
void sdm_stream_set_fs(sdm_stream_t *stream, unsigned fs);

//! Retrieve sampling frequency.
//! @param stream output stream object.
//! @return sampling frequency (Hz).
unsigned sdm_stream_get_fs(sdm_stream_t *stream);

//! Retrieve sample size.
//! @param stream output stream object.
//! @return sample size in bytes.
unsigned sdm_stream_get_sample_size(sdm_stream_t *stream);

//! Open stream.
//! @param stream output stream object.
int sdm_stream_open(sdm_stream_t *stream);

//! Close stream.
//! @param stream output stream object.
int sdm_stream_close(sdm_stream_t *stream);

//! Read samples from stream.
//! @param stream intput stream object.
//! @param samples array of samples.
//! @param sample_count number of samples.
int sdm_stream_read(sdm_stream_t *stream, int16_t* samples, unsigned sample_count);

//! Write samples to stream.
//! @param stream output stream object.
//! @param samples array of samples.
//! @param sample_count number of samples.
int sdm_stream_write(sdm_stream_t *stream, int16_t *samples, unsigned sample_count);

unsigned sdm_stream_count(sdm_stream_t *stream);

//! Retrieve the last error.
//! @param stream output stream object.
//! @return last error.
const char* sdm_stream_get_error(sdm_stream_t *stream);

//! Print stream parameters to standard output.
//! @param stream output stream object.
void sdm_stream_dump(sdm_stream_t *stream);

#endif

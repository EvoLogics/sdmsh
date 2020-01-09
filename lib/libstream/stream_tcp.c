//*************************************************************************
// Authors: Oleksiy Kebkal                                                *
//*************************************************************************

// ISO C headers.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>          /* waitpid() */
#include <sys/un.h>
#include <sys/wait.h>           /* waitpid() */

#include <stream.h>
#include <error.h>

struct private_data_t
{
    // Code of last error.
    int error;
    // Last error operation.
    const char* error_op;

    // socket handle.
    int fd;
    // socket parameters
    struct sockaddr_in saun;
};

static int stream_open_connect(sdm_stream_t *stream)
{
    struct private_data_t *pdata;

    if (!stream)
        return SDM_ERROR_STREAM;
    pdata = stream->pdata;

    if ((pdata->fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        RETURN_ERROR("socket creation", errno);

    /* TODO: add retry count here */
    if (connect(pdata->fd, (struct sockaddr *)&pdata->saun, sizeof(pdata->saun)) == -1) {
        close(pdata->fd);
        RETURN_ERROR("connecting socket", errno);
    }
    return SDM_ERROR_NONE;
}

static int stream_open_listen(sdm_stream_t *stream)
{
    struct private_data_t *pdata;
    int wait_conn_fd = -1;
    int opt;

    if (!stream)
        return SDM_ERROR_STREAM;
    pdata = stream->pdata;

    if ((wait_conn_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        pdata->error = errno;
        pdata->error_op = "socket creation error";
        goto stream_listen_error;
    }

    opt = 1;
    if (setsockopt(wait_conn_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof (opt)) < 0) {
        pdata->error = errno;
        pdata->error_op = "setting socket parameters";
        goto stream_listen_error;
    }

    if (bind(wait_conn_fd,(struct sockaddr *)&pdata->saun, sizeof(pdata->saun)) < 0) {
        pdata->error = errno;
        pdata->error_op = "binding socket";
        goto stream_listen_error;
    }

    if (listen(wait_conn_fd, 1) == -1) {
        pdata->error = errno;
        pdata->error_op = "listening socket";
        goto stream_listen_error;
    }

    pdata->fd = accept(wait_conn_fd, NULL, NULL);
    if (pdata->fd < 0) {
        pdata->error = errno;
        pdata->error_op = "accepting socket connection";
        goto stream_listen_error;
    }
  return SDM_ERROR_NONE;

stream_listen_error:
  if (wait_conn_fd >= 0) {
      close(wait_conn_fd);
  }
  return SDM_ERROR_STREAM;

}

static int stream_open(sdm_stream_t *stream)
{
    struct private_data_t *pdata;
    int rc, port;
    char *args;
    char *socket_type, *ip_s, *port_s;

    if (!stream)
        return SDM_ERROR_STREAM;
    pdata = stream->pdata;

    /* args: tcp:[connect|listen]:<ip>:<port> */
    if (stream->args == NULL)
        RETURN_ERROR("tcp arguments", EINVAL);

    args = strdup(stream->args);
    socket_type = strtok(args, ":");
    ip_s = strtok(NULL, ":");
    port_s = strtok(NULL, ":");
    if (!socket_type || !ip_s || !port_s) {
        pdata->error = EINVAL;
        pdata->error_op = "arguments parsing";
        goto stream_open_finish;
    }
    port = atoi(port_s);

    pdata->saun.sin_family = AF_INET;
    pdata->saun.sin_addr.s_addr = inet_addr(ip_s);
    pdata->saun.sin_port = htons(port);

    if (strcmp(socket_type, "connect") == 0) {
        rc = stream_open_connect(stream);
    } else if (strcmp(socket_type, "listen") == 0) {
        rc = stream_open_listen(stream);
    } else {
        pdata->error = EINVAL;
        pdata->error_op = "connection type";
    }
    if (rc < 0) {
        pdata->error = errno;
        pdata->error_op = "opening stream";
    }
  stream_open_finish:
    free(args);
    return rc;
}

static int stream_close(sdm_stream_t *stream)
{
    struct private_data_t *pdata;

    if (!stream)
        return SDM_ERROR_STREAM;
    pdata = stream->pdata;

    if (pdata->fd >= 0)
        close(pdata->fd);

    return SDM_ERROR_NONE;
}

static void stream_free(sdm_stream_t *stream)
{
    if (stream && stream->pdata) {
        free(stream->pdata);
        stream->pdata = NULL;
    }
}

static int stream_read(const sdm_stream_t *stream, int16_t* samples, unsigned sample_count)
{
    struct private_data_t *pdata;
    int rv, offset = 0;
    int requested_length = 2 * sample_count;

    if (!stream)
        return SDM_ERROR_STREAM;
    pdata = stream->pdata;

    if (stream->direction == STREAM_OUTPUT)
        RETURN_ERROR("reading from stream", ENOTSUP);

    do {
        rv = read(pdata->fd, &((char*)samples)[offset], requested_length - offset);
        if (rv > 0) {
            offset += rv;
        } else {
            if (rv < 0 && (errno == EAGAIN || errno == EINTR))
                continue;
            RETURN_ERROR("reading from stream", errno);
        }
    } while (offset < requested_length);

    if (rv == 0)
        return offset / 2;
    if (offset != requested_length)
        return SDM_ERROR_STREAM;

    return sample_count;
}

static int stream_write(sdm_stream_t *stream, void* samples, unsigned sample_count)
{
    struct private_data_t *pdata;
    int rc, offset = 0;
    int requested_length = 2 * sample_count;

    if (!stream)
        return SDM_ERROR_STREAM;
    pdata = stream->pdata;

    if (stream->direction == STREAM_INPUT)
        RETURN_ERROR("writing to stream", ENOTSUP);

    do {
        rc = write(pdata->fd, &((char*)samples)[offset], requested_length - offset);
        if (rc > 0) {
            offset += rc;
        } else {
            if (rc < 0 && (errno == EAGAIN || errno == EINTR))
                continue;
            RETURN_ERROR("writing to stream", errno);
        }
    } while (offset < requested_length);

    if (offset != requested_length)
        return SDM_ERROR_STREAM;

    return sample_count;
}

static int stream_get_errno(sdm_stream_t *stream)
{
    struct private_data_t *pdata;

    if (!stream)
        return EINVAL;
    pdata = stream->pdata;

    return pdata->error;
}

static const char* stream_strerror(sdm_stream_t *stream)
{
    struct private_data_t *pdata;

    if (!stream)
        return "No stream";
    pdata = stream->pdata;

    return strerror(pdata->error);
}

static const char* stream_get_error_op(sdm_stream_t *stream)
{
    struct private_data_t *pdata;

    if (!stream)
        return "No stream";
    pdata = stream->pdata;

    return pdata->error_op;
}

static int stream_count(sdm_stream_t* stream)
{
    struct private_data_t *pdata;

    if (!stream)
        return SDM_ERROR_STREAM;
    pdata = stream->pdata;

    RETURN_ERROR("stream count", EAFNOSUPPORT);
}

int sdm_stream_tcp_new(sdm_stream_t *stream)
{
    stream->pdata = calloc(1, sizeof(struct private_data_t));
    stream->open = stream_open;
    stream->close = stream_close;
    stream->free = stream_free;
    stream->read = stream_read;
    stream->write = stream_write;
    stream->get_errno = stream_get_errno;
    stream->strerror = stream_strerror;
    stream->get_error_op = stream_get_error_op;
    stream->count = stream_count;
    strcpy(stream->name, "TCP");

    return SDM_ERROR_NONE;
}

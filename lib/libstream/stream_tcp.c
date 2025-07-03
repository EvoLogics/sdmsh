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

struct private_data_t
{
    // Code of last error.
    int error;
    // Last error operation.
    const char* error_op;

    // socket handle.
    int sockfd;
    int clientfd;

    // socket parameters
    struct sockaddr_in saun;
};

static int stream_impl_open_connect(stream_t *stream)
{
    struct private_data_t *pdata = stream->pdata;

    pdata->clientfd = -1;
    if ((pdata->sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        STREAM_RETURN_ERROR("socket creation", errno);

    /* TODO: add retry count here */
    if (connect(pdata->sockfd, (struct sockaddr *)&pdata->saun, sizeof(pdata->saun)) == -1) {
        close(pdata->sockfd);
        STREAM_RETURN_ERROR("connecting socket", errno);
    }
    return STREAM_ERROR_NONE;
}

static int stream_impl_open_listen(stream_t *stream)
{
    struct private_data_t *pdata = stream->pdata;
    int opt;

    if ((pdata->sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        pdata->error = errno;
        pdata->error_op = "socket creation error";
        goto stream_impl_listen_error;
    }

    opt = 1;
    if (setsockopt(pdata->sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof (opt)) < 0) {
        pdata->error = errno;
        pdata->error_op = "setting socket parameters";
        goto stream_impl_listen_error;
    }

    if (bind(pdata->sockfd,(struct sockaddr *)&pdata->saun, sizeof(pdata->saun)) < 0) {
        pdata->error = errno;
        pdata->error_op = "binding socket";
        goto stream_impl_listen_error;
    }

    if (listen(pdata->sockfd, 1) == -1) {
        pdata->error = errno;
        pdata->error_op = "listening socket";
        goto stream_impl_listen_error;
    }

    pdata->clientfd = accept(pdata->sockfd, NULL, NULL);
    if (pdata->clientfd < 0) {
        pdata->error = errno;
        pdata->error_op = "accepting socket connection";
        goto stream_impl_listen_error;
    }
  return STREAM_ERROR_NONE;

stream_impl_listen_error:
  if (pdata->sockfd >= 0)
      close(pdata->sockfd);
  return STREAM_ERROR;

}

static int stream_impl_open(stream_t *stream)
{
    struct private_data_t *pdata = stream->pdata;
    int rc, port;
    char *args;
    char *socket_type, *ip_s, *port_s;

    /* args: tcp:[connect|listen]:<ip>:<port> */
    if (stream->args[0] == 0)
        STREAM_RETURN_ERROR("tcp arguments", EINVAL);

    args = strdup(stream->args);
    socket_type = strtok(args, ":");
    ip_s = strtok(NULL, ":");
    port_s = strtok(NULL, ":");
    if (!socket_type || !ip_s || !port_s) {
        pdata->error = EINVAL;
        pdata->error_op = "arguments parsing";
        rc = STREAM_ERROR;
        goto stream_impl_open_finish;
    }
    port = atoi(port_s);

    pdata->saun.sin_family = AF_INET;
    pdata->saun.sin_addr.s_addr = inet_addr(ip_s);
    pdata->saun.sin_port = htons(port);

    if (strcmp(socket_type, "connect") == 0) {
        rc = stream_impl_open_connect(stream);
    } else if (strcmp(socket_type, "listen") == 0) {
        rc = stream_impl_open_listen(stream);
    } else {
        pdata->error = EINVAL;
        pdata->error_op = "connection type";
        rc = STREAM_ERROR;
    }
    if (rc < 0) {
        pdata->error = errno;
        pdata->error_op = "opening stream";
    }
  stream_impl_open_finish:
    free(args);
    return rc;
}

static int stream_impl_close(stream_t *stream)
{
    struct private_data_t *pdata = stream->pdata;

    if (pdata->sockfd >= 0)
        close(pdata->sockfd);
    pdata->sockfd = -1;

    if (pdata->clientfd >= 0)
	    close(pdata->clientfd);
    pdata->clientfd = -1;

    return STREAM_ERROR_NONE;
}

static void stream_impl_free(stream_t *stream)
{
    free(stream->pdata);
    stream->pdata = NULL;
}

static int stream_get_fd(const stream_t *stream)
{
	struct private_data_t *pdata = stream->pdata;
	return pdata->clientfd != -1 ? pdata->clientfd : pdata->sockfd;
}

static int stream_impl_read(const stream_t *stream, int16_t* samples, unsigned sample_count)
{
    struct private_data_t *pdata = stream->pdata;
    int fd, rv, offset = 0;
    int requested_length = 2 * sample_count;

    fd = stream_get_fd(stream);

    if (stream->direction == STREAM_OUTPUT)
        STREAM_RETURN_ERROR("reading from stream", ENOTSUP);

    do {
        rv = read(fd, &((char*)samples)[offset], requested_length - offset);
        if (rv > 0) {
            offset += rv;
        } else {
            if (rv < 0 && (errno == EAGAIN || errno == EINTR))
                continue;
            STREAM_RETURN_ERROR("reading from stream", errno);
        }
    } while (offset < requested_length);

    if (rv == 0)
        return offset / 2;
    if (offset != requested_length)
        return STREAM_ERROR;

    return sample_count;
}

static int stream_impl_write(stream_t *stream, void* samples, unsigned sample_count)
{
    struct private_data_t *pdata = stream->pdata;
    int fd, rc, offset = 0;
    int requested_length = 2 * sample_count;

    fd = stream_get_fd(stream);

    if (stream->direction == STREAM_INPUT)
        STREAM_RETURN_ERROR("writing to stream", ENOTSUP);

    do {
        rc = write(fd, &((char*)samples)[offset], requested_length - offset);
        if (rc > 0) {
            offset += rc;
        } else {
            if (rc < 0 && (errno == EAGAIN || errno == EINTR))
                continue;
            STREAM_RETURN_ERROR("writing to stream", errno);
        }
    } while (offset < requested_length);

    if (offset != requested_length)
        return STREAM_ERROR;

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
    STREAM_RETURN_ERROR("stream count", EAFNOSUPPORT);
}

int stream_impl_tcp_new(stream_t *stream)
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
    strncpy(stream->name, "TCP", sizeof (stream->name));

    return STREAM_ERROR_NONE;
}

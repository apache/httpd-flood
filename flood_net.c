#include "config.h"
#include "flood_profile.h"
#include "flood_net.h"

/* Open the TCP connection to the server */
apr_socket_t* open_socket(apr_pool_t *pool, request_t *r)
{
    apr_status_t rv = 0;
    apr_sockaddr_t *destsa;
    /* FIXME: apr_socket_t == socket_t - fix profile.h */
    apr_socket_t *socket;

    if ((rv = apr_sockaddr_info_get(&destsa, r->parsed_uri->hostname, APR_INET, 
                                    r->parsed_uri->port, 0, pool)) 
                                    != APR_SUCCESS) {
        return NULL;
    }

    if ((rv = apr_socket_create(&socket, APR_INET, SOCK_STREAM,
                                pool)) != APR_SUCCESS) {
        return NULL;
    }

    if ((rv = apr_connect(socket, destsa)) != APR_SUCCESS) {
        if (APR_STATUS_IS_EINPROGRESS(rv)) {
            /* FIXME: Handle better */
            apr_socket_close(socket);
            return NULL;
        }
        else {
            /* FIXME: Handle */
            apr_socket_close(socket);
            return NULL;
        }
    }

    apr_setsocketopt(socket, APR_SO_TIMEOUT, LOCAL_SOCKET_TIMEOUT);

    return socket;
}

/* close down TCP socket */
void close_socket(apr_socket_t *s)
{
    /* FIXME: recording and other stuff here? */
    apr_socket_close(s);
}

apr_status_t read_socket(apr_socket_t *s, char *buf, int *buflen)
{
    return apr_recv(s, buf, buflen);
}

apr_status_t write_socket(apr_socket_t *s, request_t *r)
{
    apr_size_t l;
    apr_status_t e;

    l = r->rbufsize;

    e = apr_send(s, r->rbuf, &l);

    /* FIXME: Better error and allow restarts? */
    if (l != r->rbufsize)
        return APR_EGENERAL;

    return APR_SUCCESS;     
}

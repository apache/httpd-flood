#include <string.h>
#include <stdlib.h>

#include "config.h"
#include "flood_net.h"
#include "flood_net_ssl.h"
#include "flood_socket_ssl.h"

typedef struct {
    ssl_socket_t *s;
    int reopen_socket;
} ssl_socket_state_t;

apr_status_t ssl_socket_init(socket_t **sock, apr_pool_t *pool)
{
    ssl_socket_state_t *new_ssock;

    new_ssock = (ssl_socket_state_t *)apr_palloc(pool, sizeof(ssl_socket_state_t));
    if (new_ssock == NULL)
        return APR_ENOMEM;

    new_ssock->s = NULL;
    new_ssock->reopen_socket = 0;

    *sock = new_ssock;
    return APR_SUCCESS;
}

apr_status_t ssl_begin_conn(socket_t *sock, request_t *req, apr_pool_t *pool)
{
    ssl_socket_state_t *ssock = (ssl_socket_state_t *)sock;

    if (ssock->reopen_socket || ssock->s == NULL) {
        ssock->s = ssl_open_socket(pool, req);
        if (ssock->s == NULL)
            return APR_EGENERAL;
        ssock->reopen_socket = 0; /* we just opened it */
    }

    req->keepalive = 0; /* FIXME: Maybe move this into the socket_t private struct */
    return APR_SUCCESS;
}

apr_status_t ssl_send_req(socket_t *sock, request_t *req, apr_pool_t *pool)
{
    ssl_socket_state_t *ssock = (ssl_socket_state_t *)sock;
    return ssl_write_socket(ssock->s, req);
}

apr_status_t ssl_recv_resp(response_t **resp, socket_t *sock, apr_pool_t *pool)
{
    response_t *new_resp;
    apr_status_t status;
    ssl_socket_state_t *ssock = (ssl_socket_state_t *)sock;

    new_resp = apr_pcalloc(pool, sizeof(response_t));
    new_resp->rbuftype = POOL;
    new_resp->rbufsize = MAX_DOC_LENGTH;
    new_resp->rbuf = apr_pcalloc(pool, new_resp->rbufsize+1);

    status = ssl_read_socket(ssock->s, new_resp->rbuf, &new_resp->rbufsize);

    if (status != APR_SUCCESS && status != APR_EOF) {
        return status;
    }

    *resp = new_resp;

    return APR_SUCCESS;
}

/**
 * SSL implementation for end_conn.
 */
apr_status_t ssl_end_conn(socket_t *sock, request_t *req, response_t *resp)
{
    ssl_socket_state_t *ssock = (ssl_socket_state_t *)socket;

    /* debatable keepalive support */
    ssl_close_socket(ssock->s); 

    return APR_SUCCESS;
}

/**
 * SSL implementation for socket_destroy.
 */
apr_status_t ssl_socket_destroy(socket_t *socket)
{
    /* FIXME: Does anything special happen here now that it's moved
     * into ssl_conn_end() ? -aaron */
    return APR_SUCCESS;
}

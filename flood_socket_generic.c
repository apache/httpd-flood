#include <string.h>
#include <stdlib.h>

#include "config.h"
#include "flood_net.h"
#include "flood_net_ssl.h"
#include "flood_socket_generic.h"

typedef struct {
    void *s;
    int wantresponse;   /* A boolean */
    int ssl;            /* A boolean */
} generic_socket_t;

apr_status_t generic_socket_init(socket_t **sock, apr_pool_t *pool)
{
    generic_socket_t *new_gsock;

    new_gsock = (generic_socket_t *)apr_palloc(pool, sizeof(generic_socket_t));
    if (new_gsock == NULL)
        return APR_ENOMEM;
    new_gsock->s = NULL;

    *sock = new_gsock;
    return APR_SUCCESS;
}

/**
 * Generic implementation for begin_conn
 */
apr_status_t generic_begin_conn(socket_t *sock, request_t *req, apr_pool_t *pool)
{
    generic_socket_t *gsock = (generic_socket_t *)sock;
    if (strcasecmp(req->parsed_uri->scheme, "https") == 0)
        gsock->ssl = 1;
    else
        gsock->ssl = 0;

    /* The return types are not identical, so it can't be a ternary
     * operation. */
    if (gsock->ssl)
        gsock->s = ssl_open_socket(pool, req);
    else
        gsock->s = open_socket(pool, req);

    if (gsock->s == NULL)
        return APR_EGENERAL;

    req->keepalive = 0; /* FIXME: Maybe move this into flood_socket_t */
    return APR_SUCCESS;
}

/**
 * Generic implementation for send_req.
 */
apr_status_t generic_send_req(socket_t *sock, request_t *req, apr_pool_t *pool)
{
    generic_socket_t *gsock = (generic_socket_t *)sock;
    gsock->wantresponse = req->wantresponse;
    return gsock->ssl ? ssl_write_socket(gsock->s, req) :
                        write_socket(gsock->s, req);
}

/**
 * Generic implementation for recv_resp.
 */
apr_status_t generic_recv_resp(response_t **resp, socket_t *sock, apr_pool_t *pool)
{
    char b[MAX_DOC_LENGTH];
    int i;
    response_t *new_resp;
    apr_status_t status;

    generic_socket_t *gsock = (generic_socket_t *)sock;

    new_resp = apr_pcalloc(pool, sizeof(response_t));
    new_resp->rbuftype = POOL;

    if (gsock->wantresponse)
    {
        /* Ugh, we want everything. */
        int currentalloc;
        char *cp, *op;

        new_resp->rbufsize = 0;
        currentalloc = MAX_DOC_LENGTH;
        new_resp->rbuf = apr_palloc(pool, currentalloc);
        cp = new_resp->rbuf;
        do
        {
            i = MAX_DOC_LENGTH - 1;
            status = gsock->ssl ? ssl_read_socket(gsock->s, b, &i)
                                : read_socket(gsock->s, b, &i);
            if (new_resp->rbufsize + i > currentalloc)
            {
                /* You can think why this always work. */
                currentalloc *= 2;
                op = new_resp->rbuf;
                new_resp->rbuf = apr_palloc(pool, currentalloc);
                memcpy(new_resp->rbuf, op, cp - op);
                cp = new_resp->rbuf + (cp - op);
            }
            
            memcpy(cp, b, i);
            new_resp->rbufsize += i;
            cp += i;
        }
        while (status != APR_EOF && status != APR_TIMEUP);
    }
    else
    {
        /* We just want to store the first chunk read. */
        new_resp->rbufsize = MAX_DOC_LENGTH - 1;
        new_resp->rbuf = apr_palloc(pool, new_resp->rbufsize);
        status = gsock->ssl ? ssl_read_socket(gsock->s, new_resp->rbuf, 
                                              &new_resp->rbufsize) :
                              read_socket(gsock->s, new_resp->rbuf, 
                                          &new_resp->rbufsize);

        while (status != APR_EOF && status != APR_TIMEUP) {
            i = MAX_DOC_LENGTH - 1;
            status = gsock->ssl ? ssl_read_socket(gsock->s, b, &i) :
                                  read_socket(gsock->s, b, &i);
        }
        if (status != APR_SUCCESS && status != APR_EOF) {
            return status;
        }

    }

    *resp = new_resp;

    return APR_SUCCESS;
}

/**
 * Generic implementation for end_conn.
 */
apr_status_t generic_end_conn(socket_t *sock, request_t *req, response_t *resp)
{
    generic_socket_t *gsock = (generic_socket_t *)sock;
    gsock->ssl ? ssl_close_socket : close_socket(gsock->s);
    return APR_SUCCESS;
}

/**
 * Generic implementation for socket_destroy.
 */
apr_status_t generic_socket_destroy(socket_t *socket)
{
    /* The socket is closed after each request (generic doesn't
     * support keepalive), so there's nothing to do here */
    return APR_SUCCESS;
}

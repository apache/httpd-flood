/* Copyright 2001-2004 The Apache Software Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Originally developed by Aaron Bannert and Justin Erenkrantz, eBuilt.
 */

#include <apr.h>

#if APR_HAVE_STDLIB_H
#include <stdlib.h>     /* rand/strtol */
#endif
#if APR_HAVE_STRING_H
#include <string.h>
#endif

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
    apr_status_t rv;
    generic_socket_t *gsock = (generic_socket_t *)sock;

    if (strcasecmp(req->parsed_uri->scheme, "https") == 0) {
        /* If we don't have SSL, error out. */
#if FLOOD_HAS_OPENSSL
        gsock->ssl = 1;
#else
        return APR_ENOTIMPL;
#endif
    }
    else {
        gsock->ssl = 0;
    }

    /* The return types are not identical, so it can't be a ternary
     * operation. */
    if (gsock->ssl)
        gsock->s = ssl_open_socket(pool, req, &rv);
    else
        gsock->s = open_socket(pool, req, &rv);

    if (gsock->s == NULL)
        return rv;

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
    apr_size_t i;
    response_t *new_resp;
    apr_status_t status;

    generic_socket_t *gsock = (generic_socket_t *)sock;

    new_resp = apr_pcalloc(pool, sizeof(response_t));
    new_resp->rbuftype = POOL;

    if (gsock->wantresponse)
    {
        /* Ugh, we want everything. */
        apr_size_t currentalloc;
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
 * This implementation always retrieves the full response.
 * We temporarily set the "wantresponse" flag to true and
 * call generic_recv_resp() to do the real work.
 */
apr_status_t generic_fullresp_recv_resp(response_t **resp,
                                        socket_t *sock,
                                        apr_pool_t *pool)
{
    generic_socket_t *gsock = (generic_socket_t *)sock;
    int orig_wantresponse   = gsock->wantresponse;
    apr_status_t status;

    gsock->wantresponse = 1;
    status = generic_recv_resp(resp, sock, pool);
    gsock->wantresponse = orig_wantresponse;

    return status;
}

/**
 * Generic implementation for end_conn.
 */
apr_status_t generic_end_conn(socket_t *sock, request_t *req, response_t *resp)
{
    generic_socket_t *gsock = (generic_socket_t *)sock;
    gsock->ssl ? ssl_close_socket(gsock->s) : close_socket(gsock->s);
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

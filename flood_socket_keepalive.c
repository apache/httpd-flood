/* ====================================================================
 * The Apache Software License, Version 1.1
 *
 * Copyright (c) 2001 The Apache Software Foundation.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The end-user documentation included with the redistribution,
 *    if any, must include the following acknowledgment:
 *       "This product includes software developed by the
 *        Apache Software Foundation (http://www.apache.org/)."
 *    Alternately, this acknowledgment may appear in the software itself,
 *    if and wherever such third-party acknowledgments normally appear.
 *
 * 4. The names "Apache" and "Apache Software Foundation" must
 *    not be used to endorse or promote products derived from this
 *    software without prior written permission. For written
 *    permission, please contact apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache",
 *    nor may "Apache" appear in their name, without prior written
 *    permission of the Apache Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE APACHE SOFTWARE FOUNDATION OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 */

#include <string.h>
#include <stdlib.h>

#include "config.h"
#include "flood_net.h"
#include "flood_net_ssl.h"
#include "flood_socket_keepalive.h"

typedef struct {
    void *s;
    apr_pollfd_t *p;
    int reopen_socket; /* A boolean */
    int wantresponse;  /* A boolean */
    int ssl;           /* A boolean */
} keepalive_socket_t;

/**
 * Keep-alive implementation for socket_init.
 */
apr_status_t keepalive_socket_init(socket_t **sock, apr_pool_t *pool)
{
    keepalive_socket_t *new_ksock;

    new_ksock = (keepalive_socket_t *)apr_palloc(pool, sizeof(keepalive_socket_t));
    if (new_ksock == NULL)
        return APR_ENOMEM;
    new_ksock->s = NULL;
    new_ksock->p = NULL;
    new_ksock->reopen_socket = 1;
    new_ksock->wantresponse = 1;
    new_ksock->ssl = 0;

    *sock = new_ksock;
    return APR_SUCCESS;
}

/**
 * Keep-alive implementation for begin_conn.
 */
apr_status_t keepalive_begin_conn(socket_t *sock, request_t *req, apr_pool_t *pool)
{
    keepalive_socket_t *ksock = (keepalive_socket_t *)sock;

    if (ksock->reopen_socket || ksock->s == NULL) {
        if (strcasecmp(req->parsed_uri->scheme, "https") == 0)
            ksock->ssl = 1;
        else
            ksock->ssl = 0;

        /* The return types are not identical, so it can't be a ternary
         * operation. */
        if (ksock->ssl)
            ksock->s = ssl_open_socket(pool, req);
        else
            ksock->s = open_socket(pool, req);

        if (ksock->s == NULL)
            return APR_EGENERAL;

        ksock->reopen_socket = 0; /* we just opened it */
    }
    req->keepalive = 1;
    return APR_SUCCESS;
}

/**
 * Keep-alive implementation for send_req.
 */
apr_status_t keepalive_send_req(socket_t *sock, request_t *req, apr_pool_t *pool)
{
    keepalive_socket_t *ksock = (keepalive_socket_t *)sock;
    ksock->wantresponse = req->wantresponse;
    return ksock->ssl ? ssl_write_socket(ksock->s, req) :
                        write_socket(ksock->s, req);
}

static apr_status_t keepalive_load_resp(response_t *resp, 
                                        keepalive_socket_t *sock,
                                        apr_size_t remaining, apr_pool_t *pool)
{
    /* Ugh, we want everything. */
    int currentalloc, remain, i;
    char *cp, *op, b[MAX_DOC_LENGTH];
    apr_status_t status;

    if (remaining > 0)
    {
        remain = 1;
        currentalloc = remaining + resp->rbufsize;
    }
    else
    {
        remain = 0;
        currentalloc = MAX_DOC_LENGTH + resp->rbufsize;
    }

    cp = apr_palloc(pool, currentalloc);
    memcpy(cp, resp->rbuf, resp->rbufsize);
    resp->rbuf = cp;
    cp = resp->rbuf + resp->rbufsize;

    do
    {
        if (!remain)
            i = MAX_DOC_LENGTH - 1;
        else
        {
            if (remaining > MAX_DOC_LENGTH - 1)
                i = MAX_DOC_LENGTH - 1;
            else
                i = remaining;
        }

        status = sock->ssl ? ssl_read_socket(sock->s, b, &i) : 
                             read_socket(sock->s, b, &i);
        if (resp->rbufsize + i > currentalloc)
        {
            /* You can think why this always work. */
            currentalloc *= 2;
            op = resp->rbuf;
            resp->rbuf = apr_palloc(pool, currentalloc);
            memcpy(resp->rbuf, op, cp - op);
            cp = resp->rbuf + (cp - op);
        }

        memcpy(cp, b, i);
        resp->rbufsize += i;
        cp += i;
        remaining -= i;
    }
    while (status != APR_EGENERAL && status != APR_EOF && status != APR_TIMEUP && (!remain || remaining));

    return status;
}

/**
 * Keep-alive implementation for recv_resp.
 */
apr_status_t keepalive_recv_resp(response_t **resp, socket_t *sock, apr_pool_t *pool)
{
    keepalive_socket_t *ksock = (keepalive_socket_t *)sock;
    char b[MAX_DOC_LENGTH], *cl, *ecl, cls[17];
    int i;
    response_t *new_resp;
    apr_status_t status;
    long content_length = 0;

    new_resp = apr_pcalloc(pool, sizeof(response_t));
    new_resp->rbuftype = POOL;
    new_resp->rbufsize = MAX_DOC_LENGTH - 1;
    new_resp->rbuf = apr_pcalloc(pool, new_resp->rbufsize);

    status = ksock->ssl ? 
             ssl_read_socket(ksock->s, new_resp->rbuf, &new_resp->rbufsize) : 
             read_socket(ksock->s, new_resp->rbuf, &new_resp->rbufsize);

    if (status != APR_SUCCESS && status != APR_EOF) {
        return status;
    }

    /* FIXME: Deal with chunking, too */
    /* FIXME: Assume we got the full header for now. */

    /* If this exists, we aren't keepalive anymore. */
    cl = strstr(new_resp->rbuf, "Connection: Close");
    if (cl)
        new_resp->keepalive = 0; 
    else
    {
        new_resp->keepalive = 1; 
    
        cl = strstr(new_resp->rbuf, "Content-Length: ");
        if (!cl)
        {
            /* Netscape sends this.  It is technically correct as the header
             * may be mixed-case - we should be case-insensitive.  But,
             * that gets mighty expensive. */
            cl = strstr(new_resp->rbuf, "Content-length: "); 
            if (!cl)
                new_resp->keepalive = 0; 
        }

        if (cl)
        {
            cl += sizeof("Content-Length: ") - 1;
            ecl = strstr(cl, CRLF);
            if (ecl && ecl - cl < 16)
            {
                strncpy(cls, cl, ecl - cl);
                cls[ecl-cl] = '\0';
                content_length = strtol(cls, &ecl, 10);
                if (*ecl != '\0')
                    new_resp->keepalive = 0; 
            }
        }

        if (new_resp->keepalive)
        {
            /* Find where we ended */
            ecl = strstr(new_resp->rbuf, CRLF CRLF);

            /* We didn't get full headers.  Crap. */
            if (!ecl)
                new_resp->keepalive = 0; 
            {
                ecl += sizeof(CRLF CRLF) - 1;
                content_length -= new_resp->rbufsize - (ecl - (char*)new_resp->rbuf);
            } 
        }
    }
   
    if (ksock->wantresponse)
    {
        if (new_resp->keepalive)
            status = keepalive_load_resp(new_resp, ksock, content_length, pool);
        else
            status = keepalive_load_resp(new_resp, ksock, 0, pool);
    }
    else
    {
        if (new_resp->keepalive)
        {
            while (content_length && status != APR_EGENERAL &&
                   status != APR_EOF && status != APR_TIMEUP) {
                if (content_length > MAX_DOC_LENGTH - 1)
                    i = MAX_DOC_LENGTH - 1;
                else
                    i = content_length;

                status = ksock->ssl ? ssl_read_socket(ksock->s, b, &i) : 
                                      read_socket(ksock->s, b, &i);

                content_length -= i;
            }
        }
        else
        {
            while (status != APR_EGENERAL && status != APR_EOF && 
                   status != APR_TIMEUP) {
                i = MAX_DOC_LENGTH - 1;
                status = ksock->ssl ? ssl_read_socket(ksock->s, b, &i) : 
                                      read_socket(ksock->s, b, &i);
            }
        }
    }

    *resp = new_resp;

    return APR_SUCCESS;
}

/**
 * Keep-alive implementation for end_conn.
 */
apr_status_t keepalive_end_conn(socket_t *sock, request_t *req, response_t *resp)
{
    keepalive_socket_t *ksock = (keepalive_socket_t *)sock;

    if (resp->keepalive == 0) {
        ksock->ssl ? ssl_close_socket(ksock->s) : close_socket(ksock->s);
        ksock->reopen_socket = 1; /* we just closed it */
    }
        
    return APR_SUCCESS;
}

apr_status_t keepalive_socket_destroy(socket_t *sock)
{
    return APR_SUCCESS;
}

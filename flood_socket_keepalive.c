/* ====================================================================
 * The Apache Software License, Version 1.1
 *
 * Copyright (c) 2001-2003 The Apache Software Foundation.  All rights
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

#include <apr.h>
#include <apr_strings.h>

#if APR_HAVE_STDLIB_H
#include <stdlib.h>     /* rand/strtol */
#endif
#if APR_HAVE_STRING_H
#include <string.h>
#endif
#include <assert.h>

#include "config.h"
#include "flood_net.h"
#include "flood_net_ssl.h"
#include "flood_socket_keepalive.h"

#define ksock_read_socket(ksock, buf, lenaddr) \
    ksock->ssl ? ssl_read_socket(ksock->s, buf, lenaddr) : \
                 read_socket(ksock->s, buf, lenaddr)

#define ksock_write_socket(ksock, req) \
    ksock->ssl ? ssl_write_socket(ksock->s, req) : \
                 write_socket(ksock->s, req)

typedef struct {
    void *s;
    apr_pollfd_t *p;
    int reopen_socket; /* A boolean */
    int wantresponse;  /* A boolean */
    int ssl;           /* A boolean */
    method_e method;   /* The method of the request. */
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

    if (!ksock->reopen_socket && ksock->s) {
        apr_status_t e;
        e = check_socket(ksock->s, pool);
        if (e != APR_SUCCESS) {
            ksock->reopen_socket = 1;
        }
    }
    if (ksock->reopen_socket || ksock->s == NULL) {
        apr_status_t rv;
        if (strcasecmp(req->parsed_uri->scheme, "https") == 0) {
        /* If we don't have SSL, error out. */
#if FLOOD_HAS_OPENSSL
            ksock->ssl = 1;
#else
        return APR_ENOTIMPL;
#endif
        }
        else {
            ksock->ssl = 0;
        }

        /* The return types are not identical, so it can't be a ternary
         * operation. */
        if (ksock->ssl)
            ksock->s = ssl_open_socket(pool, req, &rv);
        else
            ksock->s = open_socket(pool, req, &rv);

        if (ksock->s == NULL)
            return rv;

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
    ksock->method = req->method;
    return ksock->ssl ? ssl_write_socket(ksock->s, req) :
                        write_socket(ksock->s, req);
}

static long keepalive_read_chunk_size(char *begin_chunk)
{
    char chunk[17], *end_chunk;
    long chunk_length;

    /* FIXME: Handle chunk-extension */
    end_chunk = strstr(begin_chunk, CRLF);

    if (end_chunk && end_chunk - begin_chunk < 16)
    {
        strncpy(chunk, begin_chunk, end_chunk - begin_chunk);
        chunk[end_chunk-begin_chunk] = '\0';
        /* Chunks are base-16 */
        chunk_length = strtol(chunk, &end_chunk, 16);
        if (*end_chunk == '\0')
            return chunk_length;
    }

    return 0;
}

static apr_status_t keepalive_read_chunk(response_t *resp,
                                         keepalive_socket_t *sock,
                                         int chunk_length,
                                         char **bp, int bplen)
{
    apr_status_t status = APR_SUCCESS;
    int old_length = 0;

    if (!chunk_length) {
        return status;
    }

    if (!resp->chunk || !*resp->chunk) {
        chunk_length = 0;
    }

    if (chunk_length < 0) {
        old_length = chunk_length;
        chunk_length = 0;
    }

    do {
        /* Sentinel value */
        int blen = 0;
        char *start_chunk, *end_chunk, *b;

        /* Always reset the b. */
        b = *bp;

        /* Time to read the next chunk size.  At this point,
         * we should be ready to read a CRLF followed by
         * a line that contains the next chunk size.
         */
        while (!chunk_length)
        {
            /* We are reading the next chunk and see a CRLF. */
            if (blen >= 1 && b[0] == '\r') {
                b++;
                blen--;
                if (blen >= 1 && b[0] == '\n') {
                    b++;
                    blen--;
                }
                else {
                    old_length = -1;
                }
            }

            /* If blen is 0, we're empty so read more data. */
            while (!blen)
            {
                /* Reset and read as much as we can. */
                blen = bplen;
                b = *bp;
                status = ksock_read_socket(sock, b, &blen);
                if (status != APR_SUCCESS) {
                    return status;
                }

                /* We got caught in the middle of a chunk last time. */ 
                if (old_length < 0) {
                    b -= old_length;
                    blen += old_length;
                    old_length = 0;
                }
                /* We are reading the next chunk and see a CRLF. */
                if (blen >= 2 && b[0] == '\r' && b[1] == '\n') {
                    b += 2;
                    blen -= 2;
                }
            }

            start_chunk = b;
            chunk_length = keepalive_read_chunk_size(start_chunk);

            /* last-chunk */
            if (!chunk_length)
            {
                /* See if we already read the trailer and final CRLF */
                end_chunk = strstr(b, CRLF CRLF);
                if (!end_chunk)
                {
                    /* Read as much as we can. */
                    blen = bplen;
                    b = *bp;
                    status = ksock_read_socket(sock, b, &blen);
                    if (status != APR_SUCCESS)
                        return status;
                }

                /* FIXME: If we add pipelining, we need to put
                 * the remainder back so that it can be read. */
                blen -= end_chunk - b + 4;

                return APR_SUCCESS;
            }

            /* If this fails, we're very unlikely to have read a chunk! */
            end_chunk = strstr(start_chunk, CRLF) + 2;
            blen -= end_chunk - b;

            /* Oh no, we read more than one chunk this go-around! */
            if (chunk_length <= blen) {
                b += chunk_length + (end_chunk - b);
                blen -= chunk_length;
                chunk_length = 0;
            }
            else
                chunk_length -= blen;
        }

        if (chunk_length > bplen)
            blen = bplen;
        else
            blen = chunk_length;

        status = ksock_read_socket(sock, b, &blen);

        chunk_length -= blen;
    }
    while (status == APR_SUCCESS);

    return APR_SUCCESS;
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

        status = ksock_read_socket(sock, b, &i);
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
    while (status != APR_EGENERAL && status != APR_EOF && 
           status != APR_TIMEUP && (!remain || remaining));

    return status;
}

/**
 * Keep-alive implementation for recv_resp.
 */
apr_status_t keepalive_recv_resp(response_t **resp, socket_t *sock, apr_pool_t *pool)
{
    keepalive_socket_t *ksock = (keepalive_socket_t *)sock;
    char *cl, *ecl, cls[17];
    char *current_line;
    int i;
    response_t *new_resp;
    apr_status_t status;
    long content_length = 0, chunk_length;
    const char *header;

    new_resp = apr_pcalloc(pool, sizeof(response_t));
    new_resp->rbuftype = POOL;
    new_resp->rbufsize = MAX_DOC_LENGTH - 1;
    new_resp->rbuf = apr_palloc(pool, new_resp->rbufsize);

    status = ksock_read_socket(ksock, new_resp->rbuf, &new_resp->rbufsize);

    if (status != APR_SUCCESS && status != APR_EOF) {
        return status;
    }

    /* FIXME: Assume we got the full header for now. */
    new_resp->headers = apr_table_make(pool, 25);
    current_line = new_resp->rbuf;
    do {
        char *end_of_line, *header_end, *header_key, *header_val;
        int line_length, key_length;

        end_of_line = strstr(current_line, CRLF);
        if (!end_of_line || end_of_line == current_line) {
            break;
        }
        line_length = end_of_line - current_line;

        header_end = memchr(current_line, ':', line_length);
        if (header_end) {
            key_length = header_end - current_line;
 
            header_key = apr_pstrmemdup(pool, current_line, key_length);
            header_val = apr_pstrmemdup(pool, current_line + key_length + 2,
                                        line_length - key_length - 2);
            apr_table_set(new_resp->headers, header_key, header_val);
        }
        current_line += line_length + sizeof(CRLF) - 1;
    }
    while((current_line - new_resp->rbuf) < new_resp->rbufsize);

    /* If this exists, we aren't keepalive anymore. */
    header = apr_table_get(new_resp->headers, "Connection");
    if (header && !strcasecmp(header, "Close")) {
        new_resp->keepalive = 0; 
    }
    else {
        new_resp->keepalive = 1; 
    }

    /* If we have a HEAD request, we shouldn't be receiving a body. */
    if (ksock->method == HEAD) {
        *resp = new_resp;

        return APR_SUCCESS;
    }

    header = apr_table_get(new_resp->headers, "Transfer-Encoding");
    if (header && !strcasecmp(header, "Chunked"))
    {
        new_resp->chunked = 1;
        new_resp->chunk = NULL;
        /* Find where headers ended */
        cl = current_line;

        if (cl) {
            /* Skip over the CRLF chars */
            cl += sizeof(CRLF)-1;
        }

        /* We have a partial chunk and we aren't at the end. */
        if (cl && *cl && (cl - (char*)new_resp->rbuf) < new_resp->rbufsize) {
            int remaining;
    
            do {
                if (new_resp->chunk) {
                    /* If we have enough space to skip over the ending CRLF,
                     * do so. */
                    if (chunk_length + 2 <= remaining) {
                        new_resp->chunk += chunk_length + 2;
                    }
                    else {
                        /* We got more than a chunk, but not the full CRLF. */
                        chunk_length = -(remaining - chunk_length);
                        remaining = 0;
                        break;
                    }
                }
                else {
                    new_resp->chunk = cl;
                }

                if ((new_resp->chunk - (char*)new_resp->rbuf) <
                     new_resp->rbufsize && *new_resp->chunk) {
                    char *foo;
                    chunk_length = keepalive_read_chunk_size(new_resp->chunk);
                    /* Search for the beginning of the chunk. */
                    foo = strstr(new_resp->chunk, CRLF);
                    assert(foo);
                    new_resp->chunk = foo + 2;
                    remaining = new_resp->rbufsize - 
                                    (int)(new_resp->chunk - 
                                          (char*)new_resp->rbuf);
                }
                else {
                    new_resp->chunk = NULL;
                    remaining = 0;
                }
            }
            while (remaining > chunk_length);

            chunk_length -= remaining;
        }
    }
    else
    {
        header = apr_table_get(new_resp->headers, "Content-Length");
        if (!header)
        {
            new_resp->keepalive = 0; 
        }

        if (header)
        {
            cl = (char*)header;
            ecl = cl + strlen(cl);
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
            ecl = current_line;

            /* We didn't get full headers.  Crap. */
            if (!ecl)
                new_resp->keepalive = 0; 
            {
                ecl += sizeof(CRLF) - 1;
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
        char *b = apr_palloc(pool, MAX_DOC_LENGTH);
        if (new_resp->chunked)
        {
            status = keepalive_read_chunk(new_resp, ksock, chunk_length,
                                          &b, MAX_DOC_LENGTH - 1);
        }
        else if (new_resp->keepalive)
        {
            while (content_length && status != APR_EGENERAL &&
                   status != APR_EOF && status != APR_TIMEUP) {
                if (content_length > MAX_DOC_LENGTH - 1)
                    i = MAX_DOC_LENGTH - 1;
                else
                    i = content_length;

                status = ksock_read_socket(ksock, b, &i);

                content_length -= i;
            }
        }
        else
        {
            while (status != APR_EGENERAL && status != APR_EOF && 
                   status != APR_TIMEUP) {
                i = MAX_DOC_LENGTH - 1;
                status = ksock_read_socket(ksock, b, &i);
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

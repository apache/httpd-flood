/* ====================================================================
 * The Apache Software License, Version 1.1
 *
 * Copyright (c) 2001-2004 The Apache Software Foundation.  All rights
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
 *
 * Originally developed by Aaron Bannert and Justin Erenkrantz, eBuilt.
 */

#include "config.h"
#include "flood_profile.h"
#include "flood_net.h"

/* Open the TCP connection to the server */
flood_socket_t* open_socket(apr_pool_t *pool, request_t *r,
                            apr_status_t *status)
{
    apr_status_t rv = 0;
    apr_sockaddr_t *destsa;
    flood_socket_t* fs;
    
    fs = apr_palloc(pool, sizeof(flood_socket_t));

    if ((rv = apr_sockaddr_info_get(&destsa, r->parsed_uri->hostname, APR_INET, 
                                    r->parsed_uri->port, 0, pool)) 
                                    != APR_SUCCESS) {
        if (status) {
            *status = rv;
        }
        return NULL;
    }

    if ((rv = apr_socket_create(&fs->socket, APR_INET, SOCK_STREAM,
                                APR_PROTO_TCP, pool)) != APR_SUCCESS) {
        if (status) {
            *status = rv;
        }
        return NULL;
    }

    if ((rv = apr_socket_connect(fs->socket, destsa)) != APR_SUCCESS) {
        if (APR_STATUS_IS_EINPROGRESS(rv)) {
            /* FIXME: Handle better */
            close_socket(fs);
            if (status) {
                *status = rv;
            }
            return NULL;
        }
        else if (APR_STATUS_IS_EAGAIN(rv))
        {
            /* We have run out of ports available due to TIME_WAIT exhaustion.
             * Sleep for four minutes, and try again. 
             * Note: Solaris returns EADDRNOTAVAIL, Linux returns EAGAIN.
             * XXX: Then APR'IZE THIS ALREADY
             */
            apr_sleep(4 * 60 * APR_USEC_PER_SEC);
            return open_socket(pool, r, status);
        }
        else
        {
            /* FIXME: Handle */
            close_socket(fs);
            if (status) {
                *status = rv;
            }
            return NULL;
        }
    }

    apr_socket_timeout_set(fs->socket, LOCAL_SOCKET_TIMEOUT);
    fs->read_pollset.desc_type = APR_POLL_SOCKET;
    fs->read_pollset.desc.s = fs->socket;
    fs->read_pollset.reqevents = APR_POLLIN;
    fs->read_pollset.p = pool;
    
    return fs;
}

/* close down TCP socket */
void close_socket(flood_socket_t *s)
{
    /* FIXME: recording and other stuff here? */
    apr_socket_close(s->socket);
}

apr_status_t read_socket(flood_socket_t *s, char *buf, int *buflen)
{
    apr_status_t e;
    apr_int32_t socketsRead;

    e = apr_poll(&s->read_pollset, 1, &socketsRead, LOCAL_SOCKET_TIMEOUT);
    if (e != APR_SUCCESS)
        return e;
    return apr_socket_recv(s->socket, buf, buflen);
}

apr_status_t write_socket(flood_socket_t *s, request_t *r)
{
    apr_size_t l;
    apr_status_t e;

    l = r->rbufsize;

    e = apr_socket_send(s->socket, r->rbuf, &l);

    /* FIXME: Better error and allow restarts? */
    if (l != r->rbufsize)
        return APR_EGENERAL;

    return e;
}

apr_status_t check_socket(flood_socket_t *s, apr_pool_t *pool)
{
    apr_status_t e;
    apr_int32_t socketsRead;
    apr_pollfd_t pout;
    apr_int16_t event;

    pout.desc_type = APR_POLL_SOCKET;
    pout.desc.s = s->socket;
    pout.reqevents = APR_POLLIN | APR_POLLPRI | APR_POLLERR | APR_POLLHUP | APR_POLLNVAL;
    pout.p = pool;
    
    e = apr_poll(&pout, 1, &socketsRead, 1000);
    if (socketsRead && pout.rtnevents) {
        return APR_EGENERAL;
    }
    
    return APR_SUCCESS;
}

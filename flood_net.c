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
    apr_uri_t *u;
    
    fs = apr_palloc(pool, sizeof(flood_socket_t));
    if (r->parsed_proxy_uri) {
        u = r->parsed_proxy_uri;
    }
    else {
        u = r->parsed_uri;
    }

    if ((rv = apr_sockaddr_info_get(&destsa, u->hostname, APR_INET,
                                    u->port, 0, pool)) 
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

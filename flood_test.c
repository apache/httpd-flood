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

#include <apr_general.h> /* For apr_initialize */
#include <apr_strings.h>
#include <apr_file_io.h>

#if APR_HAVE_STDLIB_H
#include <stdlib.h>     /* atexit */
#endif

#include "flood_profile.h"

/* FIXME: Base this on RCS ID? */
#define FLOOD_VERSION ".001"

#define CRLF "\r\n"

/* FIXME: replace the apr_recv logic with something sane. */
#define MAX_DOC_LENGTH 8192

#define LOCAL_SOCKET_TIMEOUT 30 * APR_USEC_PER_SEC

/* Win32 doesn't have stdout or stderr. */
apr_file_t *local_stdout, *local_stderr;

/* Should be the new apr_sms_t struct?  Not ready yet.  */
apr_pool_t *local_pool;

/* Construct a request */
static void construct_request(request_t *r)
{
    /* FIXME: Handle keepalives. */
    switch (r->method)
    {
    case GET:
        r->rbuf = apr_psprintf(local_pool, 
                               "GET %s HTTP/1.1" CRLF
                               "User-Agent: Flood/" FLOOD_VERSION CRLF
                               "Host: %s" CRLF CRLF,
                               r->parsed_uri->path, 
                               r->parsed_uri->hostinfo);
        r->rbuftype = POOL;
        r->rbufsize = strlen(r->rbuf);
        break;
    case HEAD:
        r->rbuf = apr_psprintf(local_pool, 
                               "HEAD %s HTTP/1.1" CRLF
                               "User-Agent: Flood/" FLOOD_VERSION CRLF
                               "Host: %s" CRLF CRLF,
                               r->parsed_uri->path, 
                               r->parsed_uri->hostinfo);
        r->rbuftype = POOL;
        r->rbufsize = strlen(r->rbuf);
        break;
    case POST:
        /* FIXME */
        r->rbuf = apr_psprintf(local_pool, 
                               "POST %s HTTP/1.1" CRLF
                               "User-Agent: Flood/" FLOOD_VERSION CRLF
                               "Host: %s" CRLF CRLF,
                               r->parsed_uri->path, 
                               r->parsed_uri->hostinfo);
        r->rbuftype = POOL;
        r->rbufsize = strlen(r->rbuf);
        break;
    }

}

/* Open the TCP connection to the server */
static apr_socket_t* open_socket(request_t *r)
{
    apr_status_t rv = 0;
    apr_sockaddr_t *destsa;
    /* FIXME: apr_socket_t == socket_t - fix profile.h */
    apr_socket_t *socket;

    if ((rv = apr_sockaddr_info_get(&destsa, r->parsed_uri->hostname, APR_INET, 
                                    r->parsed_uri->port, 0, local_pool)) 
                                    != APR_SUCCESS) {
        return NULL;
    }

    if ((rv = apr_socket_create(&socket, APR_INET, SOCK_STREAM, 
                                local_pool)) != APR_SUCCESS) {
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

    apr_socket_timeout_set(socket, LOCAL_SOCKET_TIMEOUT);

    return socket;
}

/* close down TCP socket */
static void close_socket(apr_socket_t *s)
{
    /* FIXME: recording and other stuff here? */
    apr_socket_close(s);
}

static apr_status_t write_socket(request_t *r, apr_socket_t *s)
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

/* FIXME: poll implementation? */
static response_t *read_socket(apr_socket_t *s)
{
    apr_status_t status;
    response_t *resp; 

    resp = apr_pcalloc(local_pool, sizeof(response_t));
    resp->rbuftype = POOL;
    resp->rbufsize = MAX_DOC_LENGTH;
    resp->rbuf = apr_pcalloc(local_pool, resp->rbufsize);

    status = apr_recv(s, resp->rbuf, &resp->rbufsize);

    if (status != APR_SUCCESS && status != APR_EOF)
        return NULL;

    return resp;
}

int main(int argc, char** argv)
{
    request_t r;
    apr_socket_t *s;

    /* FIXME: Where is Roy's change to return the global pool... */
    apr_initialize();
    atexit(apr_terminate);
   
    apr_pool_create(&local_pool, NULL);

    /* The pool should close these file descriptors */ 
    apr_file_open_stdout(&local_stdout, local_pool);
    apr_file_open_stderr(&local_stderr, local_pool);

    r.uri = "http://www.apachelabs.org/";
    r.method = GET;

    r.parsed_uri = apr_pcalloc(local_pool, sizeof(*r.parsed_uri));

    /* FIXME: This is the private copy. */
    apr_uri_parse(local_pool, r.uri, r.parsed_uri);
    if (!r.parsed_uri->port)
        r.parsed_uri->port = 80;

    apr_file_printf(local_stdout, "%s\n", r.parsed_uri->hostname);

    construct_request(&r);

    s = open_socket(&r);

    if (s)
    {
        response_t *resp;

        if (write_socket(&r, s) == APR_SUCCESS)
        {
            /* Wait .25 seconds... */
            /* FIXME: replace with poll. */
            apr_sleep(0.25 * APR_USEC_PER_SEC);

            resp = read_socket(s);

            if (resp)
                apr_file_printf(local_stdout, "%s\n", (char*)resp->rbuf);
        }
        close_socket(s);
    }
        
    return EXIT_SUCCESS;
}

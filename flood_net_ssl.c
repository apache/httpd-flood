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
 *
 * Originally developed by Aaron Bannert and Justin Erenkrantz, eBuilt.
 */

#include "config.h"
#include "flood_profile.h"
#include "flood_net.h"
#include "flood_net_ssl.h"

#if FLOOD_HAS_OPENSSL

#define OPENSSL_THREAD_DEFINES
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#include <apr_portable.h>

#define USE_RW_LOCK_FOR_SSL

struct ssl_socket_t {
    SSL_CTX *ssl_context;
    SSL *ssl_connection;
    flood_socket_t *socket;
};

apr_pool_t *ssl_pool;

#if APR_HAS_THREADS
apr_lock_t **ssl_locks;

typedef struct CRYPTO_dynlock_value { 
    apr_lock_t *lock; 
} CRYPTO_dynlock_value;

static CRYPTO_dynlock_value *ssl_dyn_create(const char* file, int line)
{
    CRYPTO_dynlock_value *l;

    l = apr_palloc(ssl_pool, sizeof(CRYPTO_dynlock_value));
#ifdef USE_RW_LOCK_FOR_SSL 
    apr_lock_create(&l->lock, APR_MUTEX, APR_INTRAPROCESS, NULL, ssl_pool);
#else
    apr_lock_create(&l->lock, APR_READWRITE, APR_INTRAPROCESS, NULL, ssl_pool);
#endif
    return l;
}

static void ssl_dyn_lock(int mode, CRYPTO_dynlock_value *l, const char *file, 
                         int line)
{
    if (mode & CRYPTO_LOCK) {
#ifdef USE_RW_LOCK_FOR_SSL 
        if (mode & CRYPTO_READ) {
            apr_lock_acquire_rw(l->lock, APR_READER);
        }
        else if (mode & CRYPTO_WRITE) {
            apr_lock_acquire_rw(l->lock, APR_WRITER);
        }
#else
        apr_lock_acquire(l->lock);
#endif
    }
    else if (mode & CRYPTO_UNLOCK) {
        apr_lock_release(l->lock);
    }
}

static void ssl_dyn_destroy(CRYPTO_dynlock_value *l, const char *file,
                            int line)
{
    apr_lock_destroy(l->lock);
}

static void ssl_lock(int mode, int n, const char *file, int line)
{
    if (mode & CRYPTO_LOCK) {
#ifdef USE_RW_LOCK_FOR_SSL 
        if (mode & CRYPTO_READ) {
            apr_lock_acquire_rw(ssl_locks[n], APR_READER);
        }
        else if (mode & CRYPTO_WRITE) {
            apr_lock_acquire_rw(ssl_locks[n], APR_WRITER);
        }
#else
        apr_lock_acquire(ssl_locks[n]);
#endif
    }
    else if (mode & CRYPTO_UNLOCK)
        apr_lock_release(ssl_locks[n]);
}

static unsigned long ssl_id(void)
{
    /* FIXME: This is lame and not portable. -aaron */
    return (unsigned long) apr_os_thread_current(); 
}
#endif

apr_status_t ssl_init_socket(apr_pool_t *pool)
{
#if APR_HAS_THREADS
    int i, numlocks;
#endif

    ssl_pool = pool;

    SSL_library_init();
    OpenSSL_add_ssl_algorithms();
    SSL_load_error_strings();
    ERR_load_crypto_strings();
#if !FLOOD_HAS_DEVRAND
    RAND_load_file(RANDFILE, -1);
#endif

#if APR_HAS_THREADS
    numlocks = CRYPTO_num_locks();
    ssl_locks = apr_palloc(pool, sizeof(apr_lock_t*)*numlocks);
    for (i = 0; i < numlocks; i++)
    {
#ifdef USE_RW_LOCK_FOR_SSL 
        apr_lock_create(&ssl_locks[i], APR_READWRITE, APR_INTRAPROCESS, NULL, 
                        ssl_pool);
#else
        apr_lock_create(&ssl_locks[i], APR_MUTEX, APR_INTRAPROCESS, NULL, 
                        ssl_pool);
#endif
    }

    CRYPTO_set_locking_callback(ssl_lock);
    CRYPTO_set_id_callback(ssl_id);

    CRYPTO_set_dynlock_create_callback(ssl_dyn_create);
    CRYPTO_set_dynlock_lock_callback(ssl_dyn_lock);
    CRYPTO_set_dynlock_destroy_callback(ssl_dyn_destroy);
#endif

    return APR_SUCCESS;
}

void ssl_read_socket_handshake(ssl_socket_t *s);

ssl_socket_t* ssl_open_socket(apr_pool_t *pool, request_t *r) 
{
    apr_os_sock_t ossock;
    int e, sslError;

    ssl_socket_t *ssl_socket = apr_pcalloc(pool, sizeof(ssl_socket_t));

    /* Open our TCP-based connection */
    ssl_socket->socket = open_socket(pool, r);
    
    if (!ssl_socket->socket)
        return NULL;

    /* Get the native OS socket. */
    apr_os_sock_get(&ossock, ssl_socket->socket->socket);

    /* Create a local context */
    ssl_socket->ssl_context = SSL_CTX_new(SSLv23_client_method());
    SSL_CTX_set_options(ssl_socket->ssl_context, SSL_OP_ALL);
#ifdef SSL_MODE_AUTO_RETRY
    /* Not all OpenSSL versions support this. */
    SSL_CTX_set_options(ssl_socket->ssl_context, SSL_MODE_AUTO_RETRY);
#endif
    /*SSL_CTX_set_default_verify_paths(ssl_socket->ssl_context);*/
    SSL_CTX_load_verify_locations(ssl_socket->ssl_context, NULL, CAPATH);

    /* Initialize the SSL connection */
    ssl_socket->ssl_connection = SSL_new(ssl_socket->ssl_context);
    SSL_set_connect_state(ssl_socket->ssl_connection);

    /* Set the descriptors */
    SSL_set_fd(ssl_socket->ssl_connection, ossock);
    e = SSL_connect(ssl_socket->ssl_connection);

    if (e)
    {
        sslError = SSL_get_error(ssl_socket->ssl_connection, e);

        switch (sslError)
        {
        case SSL_ERROR_NONE:
        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:
            /* Treat as okay. */
            break;
        default:
            ERR_print_errors_fp(stderr);
            return NULL; 
        }
    }

    return ssl_socket;
}

/* close down TCP socket */
void ssl_close_socket(ssl_socket_t *s)
{
    SSL_free(s->ssl_connection);
    SSL_CTX_free(s->ssl_context);
    close_socket(s->socket);
}

apr_status_t ssl_read_socket(ssl_socket_t *s, char *buf, int *buflen)
{
    apr_status_t e;
    int sslError, socketsRead;

    /* Wait until there is something to read. */
    if (SSL_pending(s->ssl_connection) < *buflen) {
        socketsRead = 1;
        e = apr_poll(s->socket->poll, &socketsRead, LOCAL_SOCKET_TIMEOUT);

        if (socketsRead != 1)
            return APR_TIMEUP;
    }

    e = SSL_read(s->ssl_connection, buf, *buflen);
    sslError = SSL_get_error(s->ssl_connection, e);

    switch (sslError)
    {
    case SSL_ERROR_NONE:
        *buflen = e;
        break;
    case SSL_ERROR_WANT_READ:
        ssl_read_socket(s, buf, buflen);
        break;
    case SSL_ERROR_ZERO_RETURN: /* Peer closed connection. */
        return APR_EOF; 
    case SSL_ERROR_SYSCALL: /* Look at errno. */
        if (errno == 0)
            return APR_EOF;
        /* Continue through with the error case. */   
    case SSL_ERROR_WANT_WRITE:  /* Technically, not an error. */
    default:
        ERR_print_errors_fp(stderr);
        return APR_EGENERAL; 
    }

    return APR_SUCCESS;
}

void ssl_read_socket_handshake(ssl_socket_t *s)
{
    char buf[1];
    int buflen = 1; 
    /* Wait until there is something to read. */
    int socketsRead = 1;
    apr_status_t e;
    e = apr_poll(s->socket->poll, &socketsRead, LOCAL_SOCKET_TIMEOUT);
    e = SSL_read(s->ssl_connection, buf, buflen);
}

/* Write to the socket */
apr_status_t ssl_write_socket(ssl_socket_t *s, request_t *r)
{
    apr_status_t e;
    int sslError;

    /* Returns an error. */
    e = SSL_write(s->ssl_connection, r->rbuf, r->rbufsize);

    sslError = SSL_get_error(s->ssl_connection, e);
    switch (sslError)
    {
    case SSL_ERROR_NONE:
        break;
    case SSL_ERROR_WANT_READ:
        ssl_read_socket_handshake(s);
        ssl_write_socket(s, r);
        break;
    case SSL_ERROR_WANT_WRITE:
        break;
    default:
        ERR_print_errors_fp(stderr);
        return APR_EGENERAL; 
    }

    return APR_SUCCESS;     
}

#else /* FLOOD_HAS_OPENSSL */

apr_status_t ssl_init_socket(apr_pool_t *pool)
{
    return APR_ENOTIMPL;
}

ssl_socket_t* ssl_open_socket(apr_pool_t *pool, request_t *r)
{
    return NULL;
}

void ssl_close_socket(ssl_socket_t *s)
{
}

apr_status_t ssl_write_socket(ssl_socket_t *s, request_t *r)
{
    return APR_ENOTIMPL;
}

apr_status_t ssl_read_socket(ssl_socket_t *s, char *buf, int *buflen)
{
    return APR_ENOTIMPL;
}

#endif /* FLOOD_HAS_OPENSSL */

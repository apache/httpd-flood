/* Copyright 2001-2004 Apache Software Foundation
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

#include <apr_portable.h>
#include <apr_strings.h>

#if APR_HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "config.h"
#include "flood_profile.h"
#include "flood_net.h"
#include "flood_net_ssl.h"

#if FLOOD_HAS_OPENSSL

#define OPENSSL_THREAD_DEFINES
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>

struct ssl_socket_t {
    SSL_CTX *ssl_context;
    SSL *ssl_connection;
    flood_socket_t *socket;
};

apr_pool_t *ssl_pool;

#if APR_HAS_THREADS
apr_thread_mutex_t **ssl_locks;

typedef struct CRYPTO_dynlock_value { 
    apr_thread_mutex_t *lock; 
} CRYPTO_dynlock_value;

static CRYPTO_dynlock_value *ssl_dyn_create(const char* file, int line)
{
    CRYPTO_dynlock_value *l;
    apr_status_t rv;

    l = apr_palloc(ssl_pool, sizeof(CRYPTO_dynlock_value));
    rv = apr_thread_mutex_create(&l->lock, APR_THREAD_MUTEX_DEFAULT, ssl_pool);
    if (rv != APR_SUCCESS) {
        /* FIXME: return error here */
    }
    return l;
}

static void ssl_dyn_lock(int mode, CRYPTO_dynlock_value *l, const char *file, 
                         int line)
{
    if (mode & CRYPTO_LOCK) {
        apr_thread_mutex_lock(l->lock);
    }
    else if (mode & CRYPTO_UNLOCK) {
        apr_thread_mutex_unlock(l->lock);
    }
}

static void ssl_dyn_destroy(CRYPTO_dynlock_value *l, const char *file,
                            int line)
{
    apr_thread_mutex_destroy(l->lock);
}

static void ssl_lock(int mode, int n, const char *file, int line)
{
    if (mode & CRYPTO_LOCK) {
        apr_thread_mutex_lock(ssl_locks[n]);
    }
    else if (mode & CRYPTO_UNLOCK) {
        apr_thread_mutex_unlock(ssl_locks[n]);
    }
}

static unsigned long ssl_id(void)
{
    /* FIXME: This is lame and not portable. -aaron */
    return (unsigned long) apr_os_thread_current(); 
}
#endif

/* borrowed from mod_ssl */
static int ssl_rand_choosenum(int l, int h)
{
    int i;
    char buf[50];

    srand((unsigned int)time(NULL));
    apr_snprintf(buf, sizeof(buf), "%.0f",
                (((double)(rand()%RAND_MAX)/RAND_MAX)*(h-l)));
    i = atoi(buf)+1;
    if (i < l) i = l;
    if (i > h) i = h;
    return i;
}

static void load_rand(void)
{
    unsigned char stackdata[256];
    time_t tt;
    pid_t pid;
    int l, n;

    tt = time(NULL);
    l = sizeof(time_t);
    RAND_seed((unsigned char *)&tt, l);

    pid = (pid_t)getpid();
    l = sizeof(pid_t);
    RAND_seed((unsigned char *)&pid, l);

    n = ssl_rand_choosenum(0, sizeof(stackdata)-128-1);
    RAND_seed(stackdata+n, 128);
}

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
#if ! FLOOD_HAS_DEVRAND
    load_rand();
#endif

#if APR_HAS_THREADS
    numlocks = CRYPTO_num_locks();
    ssl_locks = apr_palloc(pool, sizeof(apr_thread_mutex_t*)*numlocks);
    for (i = 0; i < numlocks; i++) {
        apr_status_t rv;

        /* Intraprocess locks don't /need/ a filename... */
        rv = apr_thread_mutex_create(&ssl_locks[i], APR_THREAD_MUTEX_DEFAULT,
                                     ssl_pool);
        if (rv != APR_SUCCESS) {
            /* FIXME: error out here */
        }
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

ssl_socket_t* ssl_open_socket(apr_pool_t *pool, request_t *r,
                              apr_status_t *status) 
{
    apr_os_sock_t ossock;
    int e, sslError;

    ssl_socket_t *ssl_socket = apr_pcalloc(pool, sizeof(ssl_socket_t));

    /* Open our TCP-based connection */
    ssl_socket->socket = open_socket(pool, r, status);
    
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
    int sslError;
    apr_int32_t socketsRead;

    /* Wait until there is something to read. */
    if (SSL_pending(s->ssl_connection) < *buflen) {
        e = apr_poll(&s->socket->read_pollset, 1, &socketsRead,
                     LOCAL_SOCKET_TIMEOUT);

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
    apr_int32_t socketsRead;
    apr_status_t e;
    e = apr_poll(&s->socket->read_pollset, 1, &socketsRead,
                 LOCAL_SOCKET_TIMEOUT);
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

ssl_socket_t* ssl_open_socket(apr_pool_t *pool, request_t *r,
                              apr_status_t *status)
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

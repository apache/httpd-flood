#include "config.h"
#include "flood_profile.h"
#include "flood_net.h"
#include "flood_net_ssl.h"

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#include <apr_portable.h>

struct ssl_socket_t {
    SSL_CTX *ssl_context;
    SSL *ssl_connection;
    apr_socket_t *socket;
    apr_pollfd_t *poll;
};

apr_status_t ssl_init_socket(apr_pool_t *pool)
{
    SSL_library_init();
    OpenSSL_add_ssl_algorithms();
    SSL_load_error_strings();
    ERR_load_crypto_strings();
    RAND_load_file("/home/jerenkrantz/.rnd", -1);

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
    apr_os_sock_get(&ossock, ssl_socket->socket);

    /* Create a local context */
    ssl_socket->ssl_context = SSL_CTX_new(SSLv23_client_method());
    SSL_CTX_set_options(ssl_socket->ssl_context, SSL_OP_ALL);
    SSL_CTX_set_options(ssl_socket->ssl_context, SSL_MODE_AUTO_RETRY);
    SSL_CTX_set_default_verify_paths(ssl_socket->ssl_context);

    /* Initialize the SSL connection */
    ssl_socket->ssl_connection = SSL_new(ssl_socket->ssl_context);
    SSL_set_connect_state(ssl_socket->ssl_connection);

    /* Set the descriptors */
    SSL_set_fd(ssl_socket->ssl_connection, ossock);
    e = SSL_connect(ssl_socket->ssl_connection);

    apr_poll_setup(&ssl_socket->poll, 1, pool);
    apr_poll_socket_add(ssl_socket->poll, ssl_socket->socket, APR_POLLIN);

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
    /* s->poll can be cleaned up when our pool dies. */
    apr_socket_close(s->socket);
}

apr_status_t ssl_read_socket(ssl_socket_t *s, char *buf, int *buflen)
{
    apr_status_t e;
    int sslError, socketsRead;

    /* Wait until there is something to read. */
    socketsRead = 1;
    e = apr_poll(s->poll, &socketsRead, LOCAL_SOCKET_TIMEOUT);
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
    case SSL_ERROR_ZERO_RETURN: /* Treat as error.  Peer closed connection. */
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
    e = apr_poll(s->poll, &socketsRead, LOCAL_SOCKET_TIMEOUT);
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

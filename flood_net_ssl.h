#ifndef __flood_socket_ssl_h
#define __flood_socket_ssl_h

#include <apr_network_io.h> /* apr_socket_t */
#include <apr_pools.h>      /* apr_pool_t */

typedef struct ssl_socket_t ssl_socket_t;

apr_status_t ssl_init_socket(apr_pool_t *pool);
ssl_socket_t* ssl_open_socket(apr_pool_t *pool, request_t *r);
void ssl_close_socket(ssl_socket_t *s);
apr_status_t ssl_write_socket(ssl_socket_t *s, request_t *r);
apr_status_t ssl_read_socket(ssl_socket_t *s, char *buf, int *buflen);

#endif  /* __flood_socket_h */

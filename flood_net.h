#ifndef __flood_socket_h
#define __flood_socket_h

#include <apr_network_io.h> /* apr_socket_t */
#include <apr_pools.h>      /* apr_pool_t */

apr_socket_t* open_socket(apr_pool_t *pool, request_t *r);
void close_socket(apr_socket_t *s);
apr_status_t write_socket(apr_socket_t *s, request_t *r);
apr_status_t read_socket(apr_socket_t *s, char *buf, int *buflen);

#endif  /* __flood_socket_h */


#ifndef __flood_socket_generic_h
#define __flood_socket_generic_h

#include <apr_pools.h>

#include "flood_profile.h"

apr_status_t generic_socket_init(socket_t **sock, apr_pool_t *pool);
apr_status_t generic_begin_conn(socket_t *sock, request_t *req, apr_pool_t *pool);
apr_status_t generic_send_req(socket_t *sock, request_t *req, apr_pool_t *pool);
apr_status_t generic_recv_resp(response_t **resp, socket_t *sock, apr_pool_t *pool);
apr_status_t generic_end_conn(socket_t *sock, request_t *req, response_t *resp);
apr_status_t generic_socket_destroy(socket_t *socket);

#endif  /* __flood_socket_generic_h */

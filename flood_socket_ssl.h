
#ifndef __flood_socket_ssl_h
#define __flood_socket_ssl_h

#include "flood_net.h"
#include "flood_net_ssl.h"
#include "flood_profile.h"

apr_status_t ssl_socket_init(socket_t **sock, apr_pool_t *pool);
apr_status_t ssl_begin_conn(socket_t *sock, request_t *req, apr_pool_t *pool);
apr_status_t ssl_send_req(socket_t *sock, request_t *req, apr_pool_t *pool);
apr_status_t ssl_recv_resp(response_t **resp, socket_t *sock, apr_pool_t *pool);
apr_status_t ssl_end_conn(socket_t *socket, request_t *req, response_t *resp);
apr_status_t ssl_socket_destroy(socket_t *socket);

#endif  /* __flood_socket_ssl_h */

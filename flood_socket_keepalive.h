
#ifndef __flood_socket_keepalive_h
#define __flood_socket_keepalive_h

apr_status_t keepalive_socket_init(socket_t **sock, apr_pool_t *pool);
apr_status_t keepalive_begin_conn(socket_t *sock, request_t *req, apr_pool_t *pool);
apr_status_t keepalive_send_req(socket_t *sock, request_t *req, apr_pool_t *pool);
apr_status_t keepalive_recv_resp(response_t **resp, socket_t *sock, apr_pool_t *pool);
apr_status_t keepalive_end_conn(socket_t *sock, request_t *req, response_t *resp);
apr_status_t keepalive_socket_destroy(socket_t *sock);

#endif  /* __flood_socket_keepalive_h */

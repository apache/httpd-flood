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

#ifndef __flood_socket_keepalive_h
#define __flood_socket_keepalive_h

apr_status_t keepalive_socket_init(socket_t **sock, apr_pool_t *pool);
apr_status_t keepalive_begin_conn(socket_t *sock, request_t *req, apr_pool_t *pool);
apr_status_t keepalive_send_req(socket_t *sock, request_t *req, apr_pool_t *pool);
apr_status_t keepalive_recv_resp(response_t **resp, socket_t *sock, apr_pool_t *pool);
apr_status_t keepalive_end_conn(socket_t *sock, request_t *req, response_t *resp);
apr_status_t keepalive_socket_destroy(socket_t *sock);

#endif  /* __flood_socket_keepalive_h */

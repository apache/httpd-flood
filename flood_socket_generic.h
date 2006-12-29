/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
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

#ifndef __flood_socket_generic_h
#define __flood_socket_generic_h

#include <apr_pools.h>

#include "flood_profile.h"

apr_status_t generic_socket_init(socket_t **sock, apr_pool_t *pool);
apr_status_t generic_begin_conn(socket_t *sock, request_t *req, apr_pool_t *pool);
apr_status_t generic_send_req(socket_t *sock, request_t *req, apr_pool_t *pool);
apr_status_t generic_fullresp_recv_resp(response_t **resp, socket_t *sock, apr_pool_t *pool);
apr_status_t generic_recv_resp(response_t **resp, socket_t *sock, apr_pool_t *pool);
apr_status_t generic_end_conn(socket_t *sock, request_t *req, response_t *resp);
apr_status_t generic_socket_destroy(socket_t *socket);

#endif  /* __flood_socket_generic_h */

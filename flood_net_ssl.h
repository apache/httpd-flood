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

#ifndef __flood_net_ssl_h
#define __flood_net_ssl_h

#include <apr_network_io.h> /* apr_socket_t */
#include <apr_pools.h>      /* apr_pool_t */

typedef struct ssl_socket_t ssl_socket_t;

apr_status_t ssl_init_socket(apr_pool_t *pool);
ssl_socket_t* ssl_open_socket(apr_pool_t *pool, request_t *r,
                              apr_status_t *status);
void ssl_close_socket(ssl_socket_t *s);
apr_status_t ssl_write_socket(ssl_socket_t *s, request_t *r);
apr_status_t ssl_read_socket(ssl_socket_t *s, char *buf, int *buflen);

#endif  /* __flood_net_socket_h */

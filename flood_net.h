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

#ifndef __flood_socket_h
#define __flood_socket_h

#include <apr_network_io.h> /* apr_socket_t */
#include <apr_poll.h>       /* apr_pollfd_t */
#include <apr_pools.h>      /* apr_pool_t */

#include "flood_profile.h"

typedef struct flood_socket_t {
    apr_socket_t *socket;
    apr_pollfd_t read_pollset;
} flood_socket_t;

flood_socket_t* open_socket(apr_pool_t *pool, request_t *r,
                            apr_status_t *status);
void close_socket(flood_socket_t *s);
apr_status_t write_socket(flood_socket_t *s, request_t *r);
apr_status_t read_socket(flood_socket_t *s, char *buf, int *buflen);
apr_status_t check_socket(flood_socket_t *s, apr_pool_t *pool);

#endif  /* __flood_socket_h */

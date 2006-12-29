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

#ifndef __FLOOD_ROUND_ROBIN_H
#define __FLOOD_ROUND_ROBIN_H

apr_status_t round_robin_profile_init(profile_t **profile,
                                      config_t *config,
                                      const char *profile_name,
                                      apr_pool_t *pool);
apr_status_t round_robin_get_next_url(request_t **request,
                                      profile_t *profile);
apr_status_t round_robin_create_req(profile_t *profile,
                                    request_t *request);
apr_status_t round_robin_postprocess(profile_t *profile,
                                     request_t *req,
                                     response_t *resp);
apr_status_t verify_200(int *verified,
                        profile_t *profile,
                        request_t *req,
                        response_t *resp);
apr_status_t verify_status_code(int *verified,
                                profile_t *profile,
                                request_t *req,
                                response_t *resp);
int round_robin_loop_condition(profile_t *profile);
apr_status_t round_robin_profile_destroy(profile_t *profile);

#endif /* __FLOOD_ROUND_ROBIN_H */

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

#include <apr_portable.h>
#include <apr_strings.h>

#if APR_HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "flood_easy_reports.h"

extern apr_file_t *local_stdout;
extern apr_file_t *local_stderr;

typedef struct easy_report_t {
    apr_pool_t *pool;
} easy_report_t;

apr_status_t easy_report_init(report_t **report, config_t *config, 
                              const char *profile_name, apr_pool_t *pool)
{
    easy_report_t *easy;

    easy = apr_palloc(pool, sizeof(easy_report_t));
   
    if (!easy)
        return APR_EGENERAL;
 
    apr_pool_create(&easy->pool, pool);

    *report = easy;
    return APR_SUCCESS;
}

apr_status_t easy_process_stats(report_t *report, int verified, request_t *req, response_t *resp, flood_timer_t *timer)
{
    easy_report_t* easy;
    char *foo;

    easy = (easy_report_t*)report;

    foo = apr_psprintf(easy->pool, "%" APR_INT64_T_FMT " %" APR_INT64_T_FMT 
                       " %" APR_INT64_T_FMT " %" APR_INT64_T_FMT 
                       " %" APR_INT64_T_FMT,
                       timer->begin, timer->connect, timer->write, 
                       timer->read, timer->close);

    switch (verified)
    {
    case FLOOD_VALID:
        foo = apr_pstrcat(easy->pool, foo, " OK", NULL);
        break;
    case FLOOD_INVALID:
        foo = apr_pstrcat(easy->pool, foo, " FAIL", NULL);
        break;
    default:
        foo = apr_psprintf(easy->pool, "%s %d", foo, verified);
    }

#if APR_HAS_THREADS
    foo = apr_psprintf(easy->pool, "%s %ld %s", foo, apr_os_thread_current(), 
                       req->uri);
#else
    foo = apr_psprintf(easy->pool, "%s %ld %s", foo, getpid(), req->uri);
#endif

    apr_file_printf(local_stdout, "%s\n", foo);

    return APR_SUCCESS;
}

apr_status_t easy_report_stats(report_t *report)
{
    return APR_SUCCESS;
}

apr_status_t easy_destroy_report(report_t *report)
{
    easy_report_t *easy;
    easy = (easy_report_t*)report;
    apr_pool_destroy(easy->pool);
    return APR_SUCCESS;
}

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

#include <apr_portable.h>
#include <apr_strings.h>

#if APR_HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "flood_report_relative_times.h"

extern apr_file_t *local_stdout;
extern apr_file_t *local_stderr;

apr_status_t relative_times_report_init(report_t **report, config_t *config, 
                              const char *profile_name, apr_pool_t *pool)
{
    return APR_SUCCESS;
}

apr_status_t relative_times_process_stats(report_t *report, int verified, request_t *req, response_t *resp, flood_timer_t *timer)
{
#define FLOOD_PRINT_BUF 256
    apr_size_t buflen;
    char buf[FLOOD_PRINT_BUF];

    buflen = apr_snprintf(buf, FLOOD_PRINT_BUF,
                          "%" APR_INT64_T_FMT " %" APR_INT64_T_FMT
                          " %" APR_INT64_T_FMT " %" APR_INT64_T_FMT " %" APR_INT64_T_FMT,
                          timer->begin,
                          timer->connect - timer->begin,
                          timer->write - timer->begin,
                          timer->read - timer->begin,
                          timer->close - timer->begin);

    switch (verified)
    {
    case FLOOD_VALID:
        apr_snprintf(buf+buflen, FLOOD_PRINT_BUF-buflen, " OK ");
        break;
    case FLOOD_INVALID:
        apr_snprintf(buf+buflen, FLOOD_PRINT_BUF-buflen, " FAIL ");
        break;
    default:
        apr_snprintf(buf+buflen, FLOOD_PRINT_BUF-buflen, " %d ", verified);
    }

    /* FIXME: this call may need to be in a critical section */
#if APR_HAS_THREADS
    apr_file_printf(local_stdout, "%s %ld %s\n", buf, apr_os_thread_current(), req->uri);
#else
    apr_file_printf(local_stdout, "%s %d %s\n", buf, getpid(), req->uri);
#endif

    return APR_SUCCESS;
}

apr_status_t relative_times_report_stats(report_t *report)
{
    return APR_SUCCESS;
}

apr_status_t relative_times_destroy_report(report_t *report)
{
    return APR_SUCCESS;
}

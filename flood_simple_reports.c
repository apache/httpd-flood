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

#include "flood_simple_reports.h"

extern apr_file_t *local_stdout;
extern apr_file_t *local_stderr;

struct simple_report_t {
    int hit_count;
    int successes;
    int failures;
};
typedef struct simple_report_t simple_report_t;

apr_status_t simple_report_init(report_t **report, config_t *config, const char *profile_name, apr_pool_t *pool)
{
    simple_report_t *sr = apr_palloc(pool, sizeof(simple_report_t));

    sr->hit_count = 0;
    sr->successes = 0; sr->failures = 0;

    *report = sr;

    return APR_SUCCESS;
}

apr_status_t simple_process_stats(report_t *report, int verified, request_t *req, response_t *resp, flood_timer_t *timer)
{
    simple_report_t *sr = (simple_report_t*)report;

    sr->hit_count++;

    if (verified == FLOOD_VALID) {
        sr->successes++;
        apr_file_printf(local_stdout, "OK %s\n", req->uri);
    } else if (verified == FLOOD_INVALID) {
        sr->failures++;
        apr_file_printf(local_stdout, "FAIL %s\n", req->uri);
    } else {
        apr_file_printf(local_stderr, "simple_process_stats(): Internal Error: 'verified' has invalid value.\n");
    }

    return APR_SUCCESS;
}

apr_status_t simple_report_stats(report_t *report)
{
    simple_report_t *sr = (simple_report_t*)report;

    apr_file_printf(local_stdout, "Report Follows ------------\n");
    apr_file_printf(local_stdout, " #OK      - %d\n", sr->successes);
    apr_file_printf(local_stdout, " #FAILED  - %d\n", sr->failures);
    apr_file_printf(local_stdout, " Total ---- %d\n", sr->hit_count);

    return APR_SUCCESS;
}

apr_status_t simple_destroy_report(report_t *report)
{
    /* FIXME: APR can't free memory, and is lame */
    return APR_SUCCESS;
}

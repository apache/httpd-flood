/* ====================================================================
 * The Apache Software License, Version 1.1
 *
 * Copyright (c) 2001 The Apache Software Foundation.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The end-user documentation included with the redistribution,
 *    if any, must include the following acknowledgment:
 *       "This product includes software developed by the
 *        Apache Software Foundation (http://www.apache.org/)."
 *    Alternately, this acknowledgment may appear in the software itself,
 *    if and wherever such third-party acknowledgments normally appear.
 *
 * 4. The names "Apache" and "Apache Software Foundation" must
 *    not be used to endorse or promote products derived from this
 *    software without prior written permission. For written
 *    permission, please contact apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache",
 *    nor may "Apache" appear in their name, without prior written
 *    permission of the Apache Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE APACHE SOFTWARE FOUNDATION OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
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

apr_status_t simple_process_stats(report_t *report, int verified, request_t *req, response_t *resp)
{
    simple_report_t *sr = (simple_report_t*)report;

    sr->hit_count++;

    if (verified == FLOOD_VALID) {
        sr->successes++;
        apr_file_printf(local_stdout, "OK %s", req->uri);
    } else if (verified == FLOOD_INVALID) {
        sr->failures++;
        apr_file_printf(local_stdout, "FAIL %s", req->uri);
    } else {
        apr_file_printf(local_stderr, "simple_process_stats(): 'verified' was an invalid value.\n");
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

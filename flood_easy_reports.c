#include "flood_easy_reports.h"
#include <apr.h>

extern apr_file_t *local_stdout;
extern apr_file_t *local_stderr;

typedef void easy_report_t;

apr_status_t easy_report_init(report_t **report, config_t *config, 
                              const char *profile_name, apr_pool_t *pool)
{
    *report = apr_palloc(pool, sizeof(easy_report_t));

    return APR_SUCCESS;
}

apr_status_t easy_process_stats(report_t *report, int verified)
{
    if (verified == FLOOD_VALID) {
        apr_file_printf(local_stdout, "%" APR_INT64_T_FMT " OK\n", 
                        apr_time_now());
    } else if (verified == FLOOD_INVALID) {
        apr_file_printf(local_stdout, "%" APR_INT64_T_FMT " FAIL\n", 
                        apr_time_now());
    } else {
        apr_file_printf(local_stdout, "%" APR_INT64_T_FMT " %d\n", 
                        apr_time_now(), verified);
    }

    return APR_SUCCESS;
}

apr_status_t easy_report_stats(report_t *report)
{
    return APR_SUCCESS;
}

apr_status_t easy_destroy_report(report_t *report)
{
    /* FIXME: APR can't free memory, and is lame */
    return APR_SUCCESS;
}

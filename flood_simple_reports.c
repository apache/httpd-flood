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

apr_status_t simple_process_stats(report_t *report, int verified)
{
    simple_report_t *sr = (simple_report_t*)report;

    sr->hit_count++;

    if (verified == FLOOD_VALID) {
        sr->successes++;
        apr_file_printf(local_stdout, "OK\n");
    } else if (verified == FLOOD_INVALID) {
        sr->failures++;
        apr_file_printf(local_stdout, "FAIL\n");
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

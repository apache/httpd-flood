#ifndef __flood_easy_reports_h
#define __flood_easy_reports_h

#include <apr_pools.h>

#include "flood_config.h"
#include "flood_profile.h"

apr_status_t easy_report_init(report_t **report, config_t *config, const char *profile_name, apr_pool_t *pool);

apr_status_t easy_process_stats(report_t *report, int verified);

apr_status_t easy_report_stats(report_t *report);

apr_status_t easy_destroy_report(report_t *report);

#endif  /* __flood_easy_reports_h */

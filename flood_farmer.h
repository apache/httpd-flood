#ifndef __flood_farmer_h
#define __flood_farmer_h

#include "flood_config.h"
#include "flood_profile.h"

/**
 * Run the given farmer name with the given config, and allocate any memory
 * needed for this process from the given pool.
 */
apr_status_t run_farmer(config_t *config, const char *farmer_name, apr_pool_t *pool);

#endif  /* __flood_farmer_h */

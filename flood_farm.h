#ifndef __flood_farm_h
#define __flood_farm_h

#include "flood_config.h"
#include "flood_farmer.h"

/**
 * Run the given farm name with the given config, and allocate any memory
 * needed for this process from the given pool.
 */
apr_status_t run_farm(config_t *config, const char *farm_name, apr_pool_t *pool);

#endif  /* __flood_farm_h */

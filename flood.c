#include <stdlib.h>      /* For atexit */
#include <unistd.h>      /* For pause */
#include <apr.h>        
#include <apr_general.h> /* For apr_initialize */
#include <apr_strings.h>
#include <apr_file_io.h>
#include <apr_pools.h>

#include "config.h"
#include "flood_profile.h"
#include "flood_farm.h"
#include "flood_farmer.h"
#include "flood_config.h"
#include "flood_net_ssl.h" /* For ssl_init_socket */

/* Win32 doesn't have stdout or stderr. */
apr_file_t *local_stdin, *local_stdout, *local_stderr;

/* Should be the new apr_sms_t struct?  Not ready yet.  */
apr_pool_t *local_pool;


#if HAVE_FORK
/* FIXME: Add Forking support */
#endif  /* HAVE_FORK */

int main(int argc, char** argv)
{
    apr_status_t stat;
    config_t *config;

    /* FIXME: Where is Roy's change to return the global pool... */
    apr_initialize();
    atexit(apr_terminate);

    apr_pool_create(&local_pool, NULL);

    /* FIXME: HHAAAAAAAAAAAAAAACCCCCCCCCCCKKKKKKKKKKK! */
    /* Should be a run-time option with SSL, but Justin hates singleton. */
    ssl_init_socket(local_pool);
   
    if (argc == 1)
        apr_file_open_stdin(&local_stdin, local_pool);
    else
    {
        apr_file_open(&local_stdin, argv[1], APR_READ, APR_OS_DEFAULT, 
                      local_pool);
    }

    apr_file_open_stdout(&local_stdout, local_pool);
    apr_file_open_stderr(&local_stderr, local_pool);

    /* parse the config */
    config = parse_config(local_stdin, local_pool);

#if HAVE_FORK
    /* do fork()ing if asked */
#endif  /* HAVE_FORK */

#if FLOOD_USE_THREADS && APR_HAS_THREADS
    if ((stat = run_farm(config, "Bingo", local_pool)) != APR_SUCCESS) {
        char buf[256];
        apr_strerror(stat, (char*) &buf, 256);
        apr_file_printf(local_stderr, "Error running test profile: %s.\n", 
                        (char*)&buf);
        exit(-1);
    }
#else
    /* just run one query */
    /* FIXME: For now this is incomplete, since we can only run a single profile
     * called "RoundRobinProfile", and we can only do it once.
     */
    if ((stat = run_farmer(config, "Joe", local_pool)) != APR_SUCCESS) {
        char buf[256];
        apr_strerror(stat, (char*) &buf, 256);
        apr_file_printf(local_stderr, "Error running test profile: %s.\n", &buf);
        exit(-1);
    }
#endif  /* FLOOD_USE_THREADS */

    /* report results -- for now just print results to stdout */
 
    return EXIT_SUCCESS;
}

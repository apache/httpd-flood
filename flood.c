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

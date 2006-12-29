/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
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

#include <apr_general.h> /* For apr_initialize */
#include <apr_strings.h>
#include <apr_file_io.h>
#include <apr_pools.h>

#if APR_HAVE_STDLIB_H
#include <stdlib.h>     /* rand/strtol */
#endif
#if APR_HAVE_UNISTD_H
#include <unistd.h>      /* For pause */
#endif

#include "config.h"
#include "flood_profile.h"
#include "flood_farm.h"
#include "flood_farmer.h"
#include "flood_config.h"

#if FLOOD_HAS_OPENSSL
#include "flood_net_ssl.h" /* For ssl_init_socket */
#endif /* FLOOD_HAS_OPENSSL */

/* Win32 doesn't have stdout or stderr. */
apr_file_t *local_stdin, *local_stdout, *local_stderr;

/* Should be the new apr_sms_t struct?  Not ready yet.  */
apr_pool_t *local_pool;

static apr_status_t set_seed(config_t *config)
{
    apr_status_t stat;
    struct apr_xml_elem *root_elem, *seed_elem;
    char *xml_seed = XML_SEED;
    unsigned int seed;

    /* get the root config node */
    if ((stat = retrieve_root_xml_elem(&root_elem, config)) != APR_SUCCESS) {
        return stat;
    }

    /* get the seed node from config */
    if ((stat = retrieve_xml_elem_child(&seed_elem, root_elem, xml_seed)) != APR_SUCCESS) {
        seed = 1; /* default if not found */
    }
    else {
        if (seed_elem->first_cdata.first && seed_elem->first_cdata.first->text) {
            char *endptr;
            seed = (unsigned int) strtoll(seed_elem->first_cdata.first->text,
                                          &endptr, 10);
            if (*endptr != '\0') {
                apr_file_printf(local_stderr,
                                "XML Node %s has invalid value '%s'.\n",
                                XML_SEED, seed_elem->first_cdata.first->text);
                return APR_EGENERAL;
            }
        }
        else {
            seed = 1;
        }
    }

    /* actually set the seed */
#if FLOOD_USE_RAND
    srand(seed);
#elif FLOOD_USE_RAND48
    srand48(seed);
#elif FLOOD_USE_RANDOM
    srandom(seed);
#endif

    return APR_SUCCESS;
}

/* check if config file version matches flood config file version */
static apr_status_t check_versions(config_t *config)
{
    apr_status_t stat;
    char *endptr = NULL;
    apr_int64_t flood_version = 0;
    apr_int64_t config_version = 0;
    struct apr_xml_elem *root_elem;

    /* we assume that CONFIG_VERSION is sane */
    flood_version = apr_strtoi64(CONFIG_VERSION, NULL, 0);

    /* get the root element */
    if ((stat = retrieve_root_xml_elem(&root_elem, config)) != APR_SUCCESS) {
        return stat;
    }

    if (root_elem->attr) {
        apr_xml_attr *attr = root_elem->attr;
        while (attr) {
            if (!strncasecmp(attr->name, XML_FLOOD_CONFIG_VERSION,
                            FLOOD_STRLEN_MAX)) {
                config_version = apr_strtoi64(attr->value, &endptr, 0);
                if (*endptr != '\0') {
                    apr_file_printf(local_stderr,
                                    "invalid config version '%s'.\n",
                                    attr->value);
                    return APR_EGENERAL;
                }
            }
            attr = attr->next;
        }
    }

    if (config_version != flood_version) {
        apr_file_printf(local_stderr,
                        "your config file version '%lli' doesn't match flood config file version '%lli'.\n",
                        config_version, flood_version);
    }

    return APR_SUCCESS;

}

int main(int argc, char** argv)
{
    apr_status_t stat;
    config_t *config;

    /* FIXME: Where is Roy's change to return the global pool... */
    apr_initialize();
    atexit(apr_terminate);

    apr_pool_create(&local_pool, NULL);

#if FLOOD_HAS_OPENSSL
    /* FIXME: HHAAAAAAAAAAAAAAACCCCCCCCCCCKKKKKKKKKKK! */
    /* Should be a run-time option with SSL, but Justin hates singleton. */
    ssl_init_socket(local_pool);
#endif /* FLOOD_HAS_OPENSSL */
   
    apr_file_open_stdout(&local_stdout, local_pool);
    apr_file_open_stderr(&local_stderr, local_pool);

    if (argc == 1) {
        apr_file_open_stdin(&local_stdin, local_pool);
    }
    else if ((stat = apr_file_open(&local_stdin, argv[1], APR_READ,
                         APR_OS_DEFAULT, local_pool)) != APR_SUCCESS)
    {
        char buf[256];
        apr_strerror(stat, (char*) &buf, sizeof(buf));
        apr_file_printf(local_stderr,
                        "Error opening configuration file: %s.\n",
                        (char*)&buf);
        exit(-1);
    }

    /* parse the config */
    config = parse_config(local_stdin, local_pool);

    if ((stat = set_seed(config)) != APR_SUCCESS) {
        char buf[256];
        apr_strerror(stat, (char*) &buf, 256);
        apr_file_printf(local_stderr, "Error running test profile: %s.\n", 
                        (char*)&buf);
        exit(-1);
    }

    if ((stat = check_versions(config)) != APR_SUCCESS) {
        exit(-1);
    }

    if ((stat = run_farm(config, "Bingo", local_pool)) != APR_SUCCESS) {
        char buf[256];
        apr_strerror(stat, (char*) &buf, 256);
        apr_file_printf(local_stderr, "Error running test profile: %s.\n", 
                        (char*)&buf);
        exit(-1);
    }

    /* report results -- for now just print results to stdout */
 
    return EXIT_SUCCESS;
}

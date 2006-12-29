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

#include <apr_pools.h>
#include <apr_errno.h>
#include <apr_strings.h>

#if APR_HAVE_STRINGS_H
#include <strings.h>    /* strncasecmp */
#endif
#if APR_HAVE_STRING_H
#include <string.h>    /* strncasecmp */
#endif
#if APR_HAVE_STDLIB_H
#include <stdlib.h>     /* strtol */
#endif
#if APR_HAVE_LIMITS_H
#include <limits.h>    /* strncasecmp */
#endif

#include "config.h"
#include "flood_profile.h"

#include "flood_farmer.h"

extern apr_file_t *local_stdout;
extern apr_file_t *local_stderr;

apr_status_t run_farmer(config_t *config, const char *farmer_name, apr_pool_t *pool)
{
    apr_status_t stat;
    int count, i, j, useprofile_count;
    char *xml_farmer, **useprofile_names;
    struct apr_xml_elem *e, *root_elem, *farmer_elem, *count_elem, *time_elem;
    apr_pool_t *farmer_pool;
    apr_time_t stop_time;

#ifdef FARMER_DEBUG
    apr_file_printf(local_stderr, "Running farmer '%s'.\n",
                    farmer_name);
#endif  /* FARMER_DEBUG */

    xml_farmer = apr_pstrdup(pool, XML_FARMER);

    /* get the root config node */
    if ((stat = retrieve_root_xml_elem(&root_elem, config)) != APR_SUCCESS) {
        return stat;
    }

    /* get farmer node from config */
    if ((stat = retrieve_xml_elem_with_childmatch(
             &farmer_elem, root_elem,
             xml_farmer, XML_FARMER_NAME, farmer_name)) != APR_SUCCESS)
        return stat;

    /* get count */
    if ((stat = retrieve_xml_elem_child(
             &count_elem, farmer_elem, XML_FARMER_COUNT)) != APR_SUCCESS) {
        /* if it's missing, default to 1 */
        count = 1;
    } else {
        if (count_elem->first_cdata.first && 
            count_elem->first_cdata.first->text) {
            count = strtol(count_elem->first_cdata.first->text, NULL, 10);
            if (count == LONG_MAX || count == LONG_MIN)
                /* error, over/under-flow */
                return errno;
        } else {
            apr_file_printf(local_stderr,
                            "Farmer '%s' has element <%s> with no value, assuming 1.\n",
                            farmer_name, XML_FARMER_COUNT);
            count = 1;
        }
    }

    /* get timer (optional) */
    stop_time = -1;
    stat = retrieve_xml_elem_child(&time_elem, farmer_elem, XML_FARMER_TIME);
    if (stat == APR_SUCCESS && time_elem->first_cdata.first && 
        time_elem->first_cdata.first->text) {
        char *endptr;
        stop_time = strtoll(time_elem->first_cdata.first->text, 
                            &endptr, 10);
        if (*endptr != '\0' || stop_time < 0)
        {   
            apr_file_printf(local_stderr,
                            "Attribute %s has invalid value %s.\n",
                            XML_FARMER_TIME, 
                            time_elem->first_cdata.first->text);
            return APR_EGENERAL;
        }
        stop_time *= APR_USEC_PER_SEC;
    }

    /* count the number of "useprofile" children */
    useprofile_count = count_xml_elem_child(farmer_elem, XML_FARMER_USEPROFILE);

    if (useprofile_count <= 0) {
        apr_file_printf(local_stderr, 
                        "Farmer '%s' has no <%s> elements to run!\n",
                        farmer_name, XML_FARMER_USEPROFILE);
        return APR_EGENERAL;
    }

    /* create each of the children and put their names in an array */
    useprofile_names = apr_palloc(pool, sizeof(char*) * (useprofile_count + 1));
    /* set the sentinel (no need for pcalloc()) */
    useprofile_names[useprofile_count] = NULL; 
    i = 0;
    for (e = farmer_elem->first_child; e; e = e->next) {
        if (strncasecmp(e->name, XML_FARMER_USEPROFILE, 
                        FLOOD_STRLEN_MAX) == 0) {
            useprofile_names[i++] = apr_pstrdup(pool, 
                                                e->first_cdata.first->text);
        }
    }

    /* now run each of the profiles */
    if (stop_time == -1)
    {
        for (i = 0; i < count; i++) {
            if ((stat = apr_pool_create(&farmer_pool, pool)) != APR_SUCCESS) {
                return stat;
            }

            for (j = 0; j < useprofile_count; j++) {
                if ((stat = run_profile(farmer_pool, config, 
                                        useprofile_names[j])) != APR_SUCCESS) {
                    return stat;
                }

                apr_pool_clear(farmer_pool);
            }

            apr_pool_destroy(farmer_pool);
        }
    }
    else
    {
        apr_time_t time;
        time = apr_time_now();
        stop_time += time;
        
        while (1)
        {
            time = apr_time_now();
            if (stop_time <= time || time >= stop_time)
                break;
            
            if ((stat = apr_pool_create(&farmer_pool, pool)) != APR_SUCCESS) {
                return stat;
            }

            for (j = 0; j < useprofile_count; j++) {
                if ((stat = run_profile(farmer_pool, config, 
                                        useprofile_names[j])) != APR_SUCCESS) {
                    return stat;
                }

                apr_pool_clear(farmer_pool);
            }

            apr_pool_destroy(farmer_pool);
        }
    }

    return APR_SUCCESS;
}

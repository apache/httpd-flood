/* Copyright 2001-2004 Apache Software Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
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

#ifndef __flood_config_h
#define __flood_config_h

#include <apr_file_io.h>
#include <apr_pools.h>
#include <apr_xml.h>

/**
 * The profile_init function (see below) reads a config_t object and
 * constructs the profile_t that will be used for this test profile.
 * XXX: For now this is simply the product of the apr XML parsing routines.
 */
typedef struct {
    apr_xml_doc *doc;
    apr_pool_t *pool;
} config_t;

/**
 * Parse the configuration from the given input file descriptor,
 * and return a config_t object. All memory allocated for the config_t
 * is taken from the given pool.
 */
config_t *parse_config(apr_file_t *in, apr_pool_t *pool);

/**
 * Count the number of children of the given element that have
 * the name "name" and return it.
 */
int count_xml_elem_child(struct apr_xml_elem *elem, const char *name);

/**
 * Simply returns the root of the parsed XML document.
 */
apr_status_t retrieve_root_xml_elem(struct apr_xml_elem **elem,
                                    const config_t *config);

/**
 * Search for the first child of the given top_elem that has
 * the given name. Return APR_SUCCESS and set elem to the found
 * element if successful, or APR_EGENERAL otherwise.
 */
apr_status_t retrieve_xml_elem_child(struct apr_xml_elem **elem,
                                     const struct apr_xml_elem *top_elem,
                                     const char *name);

/**
 * Searches the configuration (starting at 'top_elem') for a particular node
 * at the given path with a <name> of 'name'. If found
 * the node is returned in the 'elem' parameter, and the function
 * returns APR_SUCCESS. Otherwise APR_EGENERAL is returned.
 */
apr_status_t retrieve_xml_elem_with_childmatch(struct apr_xml_elem **elem,
                                               struct apr_xml_elem *top_elem,
                                               char *path,
                                               const char *child_name,
                                               const char *child_value);

#endif  /* __flood_config_h */

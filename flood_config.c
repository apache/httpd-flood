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

#include <apr_file_io.h>
#include <apr_xml.h>
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

#include "config.h"
#include "flood_config.h"

extern apr_file_t *local_stdout, *local_stderr;

config_t *parse_config(apr_file_t *in, apr_pool_t *pool)
{
    apr_xml_parser *parser;
    char buf[FLOOD_IOBUF], errorbuf[FLOOD_IOBUF]; 
    apr_xml_doc *pdoc;
    config_t *config;
    apr_status_t rv = APR_SUCCESS;
    apr_size_t buflen = 0;

#ifdef CONFIG_DEBUG
    apr_file_printf(local_stdout, "entering parse_config...\n");
#endif  /* CONFIG_DEBUG */

    /* create the parser from the pool */
    parser = apr_xml_parser_create(pool);

    while (rv == APR_SUCCESS) {
        buflen = FLOOD_IOBUF;
        rv = apr_file_read(in, buf, &buflen);

        if (rv == APR_EOF) 
            break;
        else if (rv != APR_SUCCESS) {
            apr_file_printf(local_stderr, "Error reading XML.\n");
            exit(-1);
        }

#ifdef CONFIG_DEBUG
        apr_file_printf(local_stdout, "Parsing: '%s'\n", buf);
#endif  /* CONFIG_DEBUG */

        if (apr_xml_parser_feed(parser, buf, buflen) != APR_SUCCESS) {
            apr_file_printf(local_stderr, "Error parsing XML: %s\n",
                            apr_xml_parser_geterror(parser, errorbuf, 
                                                    FLOOD_IOBUF));
            exit(-1);
        }
    }

    if (apr_xml_parser_done(parser, &pdoc) != APR_SUCCESS) {
        apr_file_printf(local_stderr, "Error after parsing XML: %s\n",
                        apr_xml_parser_geterror(parser, (char*) &errorbuf, 
                                                FLOOD_IOBUF));
        exit(-1);
    }

    /* FIXME: Actually assign the parsed XML config into 'config' */
    config = apr_pcalloc(pool, sizeof(config_t));
    config->pool = pool;
    config->doc = pdoc;

    return config;
}

/**
 * Simply returns the root of the parsed XML document.
 */
apr_status_t retrieve_root_xml_elem(struct apr_xml_elem **elem,
                                    const config_t *config)
{
    *elem = config->doc->root;
    return APR_SUCCESS;
}

/**
 * Count the number of children of the given element that have
 * the name "name" and return it.
 */
int count_xml_elem_child(struct apr_xml_elem *elem, const char *name)
{
    struct apr_xml_elem *p;
    int count = 0;

    for (p = elem->first_child; p; p = p->next) {
        if (strncasecmp(p->name, name, FLOOD_STRLEN_MAX) == 0) {
            count++;
        }
    }
    return count;
}

/**
 * Search for the first child of the given top_elem that has
 * the given name. Return APR_SUCCESS and set elem to the found
 * element if successful, or APR_EGENERAL otherwise.
 */
apr_status_t retrieve_xml_elem_child(
    struct apr_xml_elem **elem,
    const struct apr_xml_elem *top_elem,
    const char *name)
{
    struct apr_xml_elem *p;

    for (p = top_elem->first_child; p; p = p->next) {
        if (strncasecmp(p->name, name, FLOOD_STRLEN_MAX) == 0) {
            *elem = p;
            return APR_SUCCESS;
        }
    }
    return APR_EGENERAL;
}

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
                                               const char *child_val)
{
    char *pc, *last; /* path component, apr_strtok state */
    const char *lc; /* last path component */
    struct apr_xml_elem *node, *tmpnode;
    apr_status_t stat;
    char *delim = XML_ELEM_DELIM;

    /* start the search from the given top element */
    node = top_elem;

    /* break the path into the components, find the first match */
    for (pc = apr_strtok(path, delim, &last);
         pc;
         pc = apr_strtok(NULL, delim, &last)) {
        if ((stat = retrieve_xml_elem_child(&tmpnode, node, pc)) != APR_SUCCESS)
            return stat; /* it'll return here if it wasn't found */
        node = tmpnode; /* keep walking down the path */
    }

    /* save the name of this node */
    lc = node->name;

    /* search each sibling for a child node called "name" with value 'name' */
    for (tmpnode = node; tmpnode; tmpnode = tmpnode->next) {
        if ((strncasecmp(tmpnode->name, lc, FLOOD_STRLEN_MAX) == 0) &&
            tmpnode->first_child &&
            tmpnode->first_child->name &&
            (strncasecmp(
                tmpnode->first_child->name,
                child_name, FLOOD_STRLEN_MAX) == 0) &&
            (strncmp(
                tmpnode->first_child->first_cdata.first->text, /* XXX: only checks first cdata. */
                child_val, FLOOD_STRLEN_MAX) == 0))
            {
                /* we found the node */
                *elem = tmpnode;
                return APR_SUCCESS;
            }
    }
    return APR_EGENERAL;
}

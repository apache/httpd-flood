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

#include <strings.h>    /* strncasecmp */
#include <string.h>     /* strncmp */
#include <apr_file_io.h>
#include <apr_xml.h>
#include <apr_strings.h>
#include <stdlib.h>

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

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
#include <stdlib.h>     /* strtol */
#include <apr_pools.h>
#include <apr_errno.h>
#include <apr_strings.h>

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
    struct apr_xml_elem *e, *root_elem, *farmer_elem, *count_elem;
    apr_pool_t *farmer_pool;

    apr_file_printf(local_stderr, "Running farmer '%s'.\n",
                    farmer_name);

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

    farmer_pool = pool;
    if ((stat = apr_pool_create(&farmer_pool, pool)) != APR_SUCCESS) {
      return stat;
    }

    /* now run each of the profiles */
    for (i = 0; i < count; i++) {
        for (j = 0; j < useprofile_count; j++) {
            if ((stat = run_profile(farmer_pool, config, 
                                    useprofile_names[j])) != APR_SUCCESS) {
                return stat;
            }

	    apr_pool_clear(farmer_pool);
        }
    }

    apr_pool_destroy(farmer_pool);

    return APR_SUCCESS;
}

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

    /* now run each of the profiles */
    for (i = 0; i < count; i++) {
        for (j = 0; j < useprofile_count; j++) {
            if ((stat = run_profile(pool, config, 
                                    useprofile_names[j])) != APR_SUCCESS) {
                return stat;
            }
        }
    }

    return APR_SUCCESS;
}

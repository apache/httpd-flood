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

#include <stdlib.h>
#include <apr_file_io.h>
#include <apr_strings.h>
#include <apr_errno.h>
#include <string.h>

#include "config.h"
#include "flood_profile.h"
#include "flood_config.h"
#include "flood_net.h"
#include "flood_net_ssl.h"

#include "flood_round_robin.h"
#include "flood_simple_reports.h"
#include "flood_easy_reports.h"

extern apr_file_t *local_stdout;
extern apr_file_t *local_stderr;

struct profile_event_handler_t {
    const char *handler_name;
    const char *impl_name;
    void *handler;
};
typedef struct profile_event_handler_t profile_event_handler_t;

/**
 * Generic implementation for profile_init.
 */
static apr_status_t generic_profile_init(profile_t **profile, config_t *config, const char * profile_name, apr_pool_t *pool)
{
    return APR_ENOTIMPL;
}

/**
 * Generic implementation for report_init.
 */
static apr_status_t generic_report_init(report_t **report, config_t *config, const char *profile_name, apr_pool_t *pool)
{
    /* by default, don't generate a report */
    *report = NULL;
    return APR_SUCCESS;
}

/**
 * Generic implementation for get_next_url.
 */
static apr_status_t generic_get_next_url(request_t **request, profile_t *profile)
{
    return APR_ENOTIMPL;
}

/**
 * Generic implementation for send_req.
 */
static apr_status_t generic_send_req(socket_t **sock, request_t *req, apr_pool_t *pool)
{
    apr_socket_t *new_sock = open_socket(pool, req);
    write_socket(new_sock, req);
    *sock = new_sock;
    return APR_SUCCESS;
}

/**
 * Generic implementation for recv_resp.
 */
static apr_status_t generic_recv_resp(response_t **resp, socket_t *sock, apr_pool_t *pool)
{
    char b[MAX_DOC_LENGTH];
    int i;
    response_t *new_resp;
    apr_status_t status;

    apr_socket_t *s = (apr_socket_t*)sock;

    new_resp = apr_pcalloc(pool, sizeof(response_t));
    new_resp->rbuftype = POOL;
    new_resp->rbufsize = MAX_DOC_LENGTH - 1;
    new_resp->rbuf = apr_pcalloc(pool, new_resp->rbufsize);

    status = read_socket(s, new_resp->rbuf, &new_resp->rbufsize);

    if (status != APR_SUCCESS && status != APR_EOF) {
        return status;
    }

    while (status != APR_EOF) {
        i = MAX_DOC_LENGTH - 1;
        status = read_socket(s, b, &i);
    }

    *resp = new_resp;

    return APR_SUCCESS;
}

/**
 * Generic implementation for postprocess.
 */
static apr_status_t generic_postprocess(profile_t *p, request_t *req, response_t *resp)
{
    return APR_SUCCESS;
}

/**
 * Generic implementation for verify_resp.
 */
static apr_status_t generic_verify_resp(int *verified, profile_t *profile, request_t *req, response_t *resp)
{
    return APR_ENOTIMPL;
}

/**
 * Generic implementation for process_stats.
 */
static apr_status_t generic_process_stats(report_t *report, int verified)
{
    /* by default report nothing */
    return APR_SUCCESS;
}

/**
 * Generic implementation for loop_condition.
 */
static int generic_loop_condition(profile_t *p)
{
    return 0; /* by default, don't loop */
}

/**
 * Generic implementation for request_destroy.
 */
static apr_status_t generic_request_destroy(request_t *req)
{
    return APR_SUCCESS;
}

/**
 * Generic implementation for response_destroy.
 */
static apr_status_t generic_response_destroy(response_t *resp)
{
    return APR_SUCCESS;
}

/**
 * Generic implementation for socket_destroy.
 */
static apr_status_t generic_socket_destroy(socket_t *socket)
{
    apr_socket_t *s = (apr_socket_t*)socket;

    /* this is a generic recv resp, no keepalive support */
    close_socket(s); 

    return APR_SUCCESS;
}

/**
 * Generic implementation for report_stats.
 */
static apr_status_t generic_report_stats(report_t *report)
{
    /* nothing to report, but not a failure */
    return APR_SUCCESS;
}

/**
 * Generic implementation for destroy_report.
 */
static apr_status_t generic_destroy_report(report_t *report)
{
    return APR_SUCCESS;
}

/**
 * Generic implementation for profile_destroy.
 */
static apr_status_t generic_profile_destroy(profile_t *p)
{
    return APR_SUCCESS;
}

static apr_status_t ssl_send_req(socket_t **sock, request_t *req, apr_pool_t *pool)
{
    ssl_socket_t *s = ssl_open_socket(pool, req);

    if (!s)
        return APR_EGENERAL;

    ssl_write_socket(s, req);

    *sock = s;

    return APR_SUCCESS;
}

static apr_status_t ssl_recv_resp(response_t **resp, socket_t *sock, apr_pool_t *pool)
{
    response_t *new_resp;
    apr_status_t status;
    ssl_socket_t *s = (ssl_socket_t*)sock;

    new_resp = apr_pcalloc(pool, sizeof(response_t));
    new_resp->rbuftype = POOL;
    new_resp->rbufsize = MAX_DOC_LENGTH;
    new_resp->rbuf = apr_pcalloc(pool, new_resp->rbufsize+1);

    status = ssl_read_socket(s, new_resp->rbuf, &new_resp->rbufsize);

    if (status != APR_SUCCESS && status != APR_EOF) {
        return status;
    }

    *resp = new_resp;

    return APR_SUCCESS;
}

/**
 * SSL implementation for socket_destroy.
 */
static apr_status_t ssl_socket_destroy(socket_t *socket)
{
    ssl_socket_t *s = (ssl_socket_t*)socket;

    /* debatable keepalive support */
    ssl_close_socket(s); 

    return APR_SUCCESS;
}

const char * profile_event_handler_names[] = {
    "profile_init",
    "report_init",
    "get_next_url",
    "send_req",
    "recv_resp",
    "postprocess",
    "verify_resp",
    "process_stats",
    "loop_condition",
    "request_destroy",
    "response_destroy",
    "socket_destroy",
    "report_stats",
    "destroy_report",
    "profile_destroy",
    NULL
};

profile_event_handler_t profile_event_handlers[] = {
    {"profile_init",     "generic_profile_init",         &generic_profile_init},
    {"report_init",      "generic_report_init",          &generic_report_init},
    {"get_next_url",     "generic_get_next_url",         &generic_get_next_url},
    {"send_req",         "generic_send_req",             &generic_send_req},
    {"recv_resp",        "generic_recv_resp",            &generic_recv_resp},
    {"postprocess",      "generic_postprocess",          &generic_postprocess},
    {"verify_resp",      "generic_verify_resp",          &generic_verify_resp},
    {"process_stats",    "generic_process_stats",        &generic_process_stats},
    {"loop_condition",   "generic_loop_condition",       &generic_loop_condition},
    {"request_destroy",  "generic_request_destroy",      &generic_request_destroy},
    {"response_destroy", "generic_response_destroy",     &generic_response_destroy},
    {"socket_destroy",   "generic_socket_destroy",       &generic_socket_destroy},
    {"report_stats",     "generic_report_stats",         &generic_report_stats},
    {"destroy_report",   "generic_destroy_report",       &generic_destroy_report},
    {"profile_destroy",  "generic_profile_destroy",      &generic_profile_destroy},

/* Alternative Implementations that are currently available: */

/* SSL support */
    {"send_req",         "ssl_send_req",             &ssl_send_req},
    {"recv_resp",        "ssl_recv_resp",            &ssl_recv_resp},
    {"socket_destroy",   "ssl_socket_destroy",       &ssl_socket_destroy},

/* Round Robin */
    {"profile_init",     "round_robin_profile_init",     &round_robin_profile_init},
    {"get_next_url",     "round_robin_get_next_url",     &round_robin_get_next_url},
    {"postprocess",      "round_robin_postprocess",      &round_robin_postprocess},
    {"loop_condition",   "round_robin_loop_condition",   &round_robin_loop_condition},
    {"profile_destroy",  "round_robin_profile_destroy",  &round_robin_profile_destroy},

/* Verification by OK/200 */
    {"verify_resp",      "verify_200",                   &verify_200},

/* Simple Reports */
    {"report_init",      "simple_report_init",           &simple_report_init},
    {"process_stats",    "simple_process_stats",         &simple_process_stats},
    {"report_stats",     "simple_report_stats",          &simple_report_stats},
    {"destroy_report",   "simple_destroy_report",        &simple_destroy_report},

/* Easy Reports */
    {"report_init",      "easy_report_init",             &easy_report_init},
    {"process_stats",    "easy_process_stats",           &easy_process_stats},
    {"report_stats",     "easy_report_stats",            &easy_report_stats},
    {"destroy_report",   "easy_destroy_report",          &easy_destroy_report},

    {NULL} /* sentinel value */
};

/**
 * Assign the appropriate implementation to the profile_events_t handler
 * for the given function name and overriden function name.
 * Returns APR_SUCCESS if an appropriate handler was found and assigned, or
 * returns APR_ENOTIMPL if no match was found.
 */
static apr_status_t assign_profile_event_handler(profile_events_t *events,
                                                 const char *handler_name,
                                                 const char *impl_name)
{
    profile_event_handler_t *p;

    for (p = &profile_event_handlers[0];
         p;
         p++) {
        /* these are case insensitive (both key and value) for the sake of simplicity */
        if (strncasecmp(impl_name, (*p).impl_name, FLOOD_STRLEN_MAX) == 0) {
            if (strncasecmp(handler_name, (*p).handler_name, FLOOD_STRLEN_MAX) == 0) {
                /* we got a match, assign it */

                /* stupid cascading if, no big deal since it only happens at startup */
                if (strncasecmp(handler_name, "profile_init", FLOOD_STRLEN_MAX) == 0) {
                    events->profile_init = (*p).handler;
                } else if (strncasecmp(handler_name, "report_init", FLOOD_STRLEN_MAX) == 0){ 
                    events->report_init = (*p).handler;
                } else if (strncasecmp(handler_name, "get_next_url", FLOOD_STRLEN_MAX) == 0){ 
                    events->get_next_url = (*p).handler;
                } else if (strncasecmp(handler_name, "send_req", FLOOD_STRLEN_MAX) == 0) {
                    events->send_req = (*p).handler;
                } else if (strncasecmp(handler_name, "recv_resp", FLOOD_STRLEN_MAX) == 0) {
                    events->recv_resp = (*p).handler;
                } else if (strncasecmp(handler_name, "postprocess", FLOOD_STRLEN_MAX) == 0) {
                    events->postprocess = (*p).handler;
                } else if (strncasecmp(handler_name, "verify_resp", FLOOD_STRLEN_MAX) == 0) {
                    events->verify_resp = (*p).handler;
                } else if (strncasecmp(handler_name, "process_stats", FLOOD_STRLEN_MAX) == 0) {
                    events->process_stats = (*p).handler;
                } else if (strncasecmp(handler_name, "loop_condition", FLOOD_STRLEN_MAX) == 0) {
                    events->loop_condition = (*p).handler;
                } else if (strncasecmp(handler_name, "request_destroy", FLOOD_STRLEN_MAX) == 0) {
                    events->request_destroy = (*p).handler;
                } else if (strncasecmp(handler_name, "response_destroy", FLOOD_STRLEN_MAX) == 0) {
                    events->response_destroy = (*p).handler;
                } else if (strncasecmp(handler_name, "socket_destroy", FLOOD_STRLEN_MAX) == 0) {
                    events->socket_destroy = (*p).handler;
                } else if (strncasecmp(handler_name, "report_stats", FLOOD_STRLEN_MAX) == 0) {
                    events->report_stats = (*p).handler;
                } else if (strncasecmp(handler_name, "destroy_report", FLOOD_STRLEN_MAX) == 0) {
                    events->destroy_report = (*p).handler;
                } else if (strncasecmp(handler_name, "profile_destroy", FLOOD_STRLEN_MAX) == 0) {
                    events->profile_destroy = (*p).handler;
                } else {
                    /* some internal error, our static structs don't match up */
                    return APR_EGENERAL;
                }
                return APR_SUCCESS;
            } else {
                /* invalid implementation for this handler */
                return APR_ENOTIMPL; /* XXX: There's probably a better return val than this? */
            }
        }
    }
    return APR_ENOTIMPL; /* no implementation found */
}

/**
 * Construct a profile_event_handler_t struct, with the "generic"
 * implementations in place by default.
 */
static apr_status_t create_profile_events(profile_events_t **events, apr_pool_t *pool)
{
    apr_status_t stat;
    profile_events_t *new_events;

    if ((new_events = apr_pcalloc(pool, sizeof(profile_events_t))) == NULL)
        return APR_ENOMEM;

    if ((stat = assign_profile_event_handler(new_events,
             "profile_init",
             "generic_profile_init")) != APR_SUCCESS)
        return stat;
    if ((stat = assign_profile_event_handler(new_events,
             "report_init",
             "generic_report_init")) != APR_SUCCESS)
        return stat;
    if ((stat = assign_profile_event_handler(new_events,
             "get_next_url",
             "generic_get_next_url")) != APR_SUCCESS)
        return stat;
    if ((stat = assign_profile_event_handler(new_events,
             "send_req",
             "generic_send_req")) != APR_SUCCESS)
        return stat;
    if ((stat = assign_profile_event_handler(new_events,
             "recv_resp",
             "generic_recv_resp")) != APR_SUCCESS)
        return stat;
    if ((stat = assign_profile_event_handler(new_events,
             "verify_resp",
             "generic_verify_resp")) != APR_SUCCESS)
        return stat;
    if ((stat = assign_profile_event_handler(new_events,
             "postprocess",
             "generic_postprocess")) != APR_SUCCESS)
        return stat;
    if ((stat = assign_profile_event_handler(new_events,
             "process_stats",
             "generic_process_stats")) != APR_SUCCESS)
        return stat;
    if ((stat = assign_profile_event_handler(new_events,
             "loop_condition",
             "generic_loop_condition")) != APR_SUCCESS)
        return stat;
    if ((stat = assign_profile_event_handler(new_events,
             "request_destroy",
             "generic_request_destroy")) != APR_SUCCESS)
        return stat;
    if ((stat = assign_profile_event_handler(new_events,
             "response_destroy",
             "generic_response_destroy")) != APR_SUCCESS)
        return stat;
    if ((stat = assign_profile_event_handler(new_events,
             "socket_destroy",
             "generic_socket_destroy")) != APR_SUCCESS)
        return stat;
    if ((stat = assign_profile_event_handler(new_events,
             "report_stats",
             "generic_report_stats")) != APR_SUCCESS)
        return stat;
    if ((stat = assign_profile_event_handler(new_events,
             "destroy_report",
             "generic_destroy_report")) != APR_SUCCESS)
        return stat;
    if ((stat = assign_profile_event_handler(new_events,
             "profile_destroy",
             "generic_profile_destroy")) != APR_SUCCESS)
        return stat;

    *events = new_events;

    return APR_SUCCESS;
}

/**
 * Initializes the profile_events_t structure according to what we
 * find in the given configuration. Dynamically allocated memory
 * is pulled from the given pool.
 */
apr_status_t initialize_events(profile_events_t **events, const char * profile_name, config_t *config, apr_pool_t *pool)
{
    apr_status_t stat;
    const char **p;
    profile_events_t *new_events;
    struct apr_xml_elem *root_elem, *profile_elem, *profile_event_elem;
    char *xml_profile;

    xml_profile = apr_pstrdup(pool, XML_PROFILE);

    if ((stat = retrieve_root_xml_elem(&root_elem, config)) != APR_SUCCESS) {
        return stat;
    }
    
    if ((stat = create_profile_events(&new_events, pool)) != APR_SUCCESS) {
        return stat;
    }

    /* retrieve our profile xml element */
    if ((stat = retrieve_xml_elem_with_childmatch(
             &profile_elem, root_elem,
             xml_profile, "name", profile_name)) != APR_SUCCESS)
        return stat;

    /* For each event in the profile_events_t struct, allow the config
     * parameters to override an implementation.
     */
    for (p = &profile_event_handler_names[0]; *p; p++) {
        if ((stat = retrieve_xml_elem_child(
                 &profile_event_elem,
                 profile_elem,
                 *p)) == APR_SUCCESS) {

            /* search for the implementation in our tables and assign it */
            if ((stat = assign_profile_event_handler(
                     new_events,
                     *p,
                     profile_event_elem->first_cdata.first->text)) != APR_SUCCESS) {
#ifdef PROFILE_DEBUG
                apr_file_printf(local_stdout, "Profile '%s' failed to override '%s' with '%s'.\n",
                                profile_name, *p, profile_event_elem->first_cdata.first->text);
#endif /* PROFILE_DEBUG */
                return stat;
            } else {
#ifdef PROFILE_DEBUG
                apr_file_printf(local_stdout, "Profile '%s' overrides '%s' with '%s'.\n",
                                profile_name, *p, profile_event_elem->first_cdata.first->text);
#endif /* PROFILE_DEBUG */
            }            

        } else {
#ifdef PROFILE_DEBUG
            apr_file_printf(local_stdout, "Profile '%s' uses default '%s'.\n",
                            profile_name, *p);
#endif /* PROFILE_DEBUG */
        }
    }
    
    *events = new_events;
    
    return APR_SUCCESS;
}

/**
 * Essential guts of the main test loop -- a single run of a test profile:
 */
apr_status_t run_profile(apr_pool_t *pool, config_t *config, const char * profile_name)
{
    profile_events_t *events;
    profile_t *profile;
    report_t *report;
    request_t *req;
    response_t *resp;
    socket_t *socket;
    apr_status_t stat;
    int verified;

    /* init to NULL for the sake of our error checking */
    events = NULL;
    profile = NULL;
    req = NULL;
    resp = NULL;
    socket = NULL;
    verified = FLOOD_INVALID;

    /* assign the implementations (function pointers) */
    if ((stat = initialize_events(&events, profile_name, config, pool)) != APR_SUCCESS) {
        return stat;
    }

    if (events == NULL) {
        apr_file_printf(local_stderr, "Error initializing test profile.\n");
        return APR_EGENERAL; /* FIXME: What error code to return? */
    }

    /* initialize this profile */
    if ((stat = events->profile_init(&profile, config, profile_name, pool)) != APR_SUCCESS)
        return stat;

    /* initialize this report */
    if ((stat = events->report_init(&report, config, profile_name, pool)) != APR_SUCCESS)
        return stat;

    do {
        if ((stat = events->get_next_url(&req, profile)) != APR_SUCCESS)
            return stat;

        /* sample timer "main" */

        if ((stat = events->send_req(&socket, req, pool)) != APR_SUCCESS) {
            apr_file_printf(local_stderr, "send request failed.\n");
            return stat;
        }

        /* sample timer "send_req" */

        if ((stat = events->recv_resp(&resp, socket, pool)) != APR_SUCCESS) {
            apr_file_printf(local_stderr, "receive request failed.\n");
            return stat;
        }

        /* sample timer "recv_resp" */

        if ((stat = events->postprocess(profile, req, resp)) != APR_SUCCESS) {
            apr_file_printf(local_stderr, "postprocessing failed.\n");
            return stat;
        }

        if ((stat = events->verify_resp(&verified, profile, req, resp)) != APR_SUCCESS) {
            apr_file_printf(local_stderr, "Error while verifying query.\n");
            return stat;
        }

        /* sample timer "full_resp" */

        if ((stat = events->process_stats(report, verified)) != APR_SUCCESS) {
            apr_file_printf(local_stderr, "Unable to process statistics.\n");
            return stat;
        }

        if ((stat = events->request_destroy(req)) != APR_SUCCESS) {
            apr_file_printf(local_stderr, "Error cleaning up request.\n");
            return stat;
        }

        if ((stat = events->response_destroy(resp)) != APR_SUCCESS) {
            apr_file_printf(local_stderr, "Error cleaning up Response.\n");
            return stat;
        }

        if ((stat = events->socket_destroy(socket)) != APR_SUCCESS) {
            apr_file_printf(local_stderr, "Error cleaning up Socket.\n");
            return stat;
        }
	    

    } while (events->loop_condition(profile));

    if ((stat = events->report_stats(report)) != APR_SUCCESS) {
        apr_file_printf(local_stderr, "Unable to report statistics.\n");
        return stat;
    }

    if (events->destroy_report(report) != APR_SUCCESS) {
        apr_file_printf(local_stderr, "Error cleaning up report '%s'.\n", profile_name);
        return APR_EGENERAL; /* FIXME: What error code to return? */
    }

    if (events->profile_destroy(profile) != APR_SUCCESS) {
        apr_file_printf(local_stderr, "Error cleaning up profile '%s'.\n", profile_name);
        return APR_EGENERAL; /* FIXME: What error code to return? */
    }

    return APR_SUCCESS;
}

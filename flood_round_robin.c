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

#include <flood_profile.h>

#include <stdlib.h>
#include <string.h>

#include <apr_file_io.h>
#include <apr_network_io.h>
#include <apr_strings.h>
#include <apr_uri.h>

#include "config.h"
#include "flood_net.h"

extern apr_file_t *local_stdout;
extern apr_file_t *local_stderr;

typedef struct {
    char *url;
    method_e method;
    char *payload;
    apr_int64_t delay;
} url_t;

typedef struct cookie_t {
    char *name;
    char *value;
    char *path;
    char *expires;
    char *raw;
    struct cookie_t *next;
} cookie_t;

typedef struct {
    apr_pool_t *pool;

    int execute_rounds;

    int urls;
    url_t *url;

    cookie_t *cookie;

    int current_round;
    int current_url;

} round_robin_profile_t;

/* Construct a request */
apr_status_t round_robin_create_req(profile_t *profile, request_t *r)
{
    round_robin_profile_t *p;
    char *cookies;
    cookie_t *cook;
   
    p = (round_robin_profile_t*)profile; 

    /* FIXME: This algorithm sucks. */
    if (p->cookie)
    {
        cookies = apr_pstrdup(p->pool, "Cookie: ");
        cook = p->cookie;
        while (cook)
        {
            if (cook != p->cookie)
                cookies = apr_pstrcat(p->pool, cookies, ";");

            cookies = apr_pstrcat(p->pool, cookies, cook->name, "=", 
                                  cook->value, NULL);
            cook = cook->next; 
        }
        cookies = apr_pstrcat(p->pool, cookies, CRLF, NULL);
    }
    else
        cookies = "";

    switch (r->method)
    {
    case GET:
        r->rbuf = apr_psprintf(r->pool, 
                               "GET %s%s%s HTTP/1.1" CRLF
                               "User-Agent: Flood/" FLOOD_VERSION CRLF
                               "Connection: %s" CRLF
                               "Host: %s" CRLF 
                               "%s" CRLF,
                               r->parsed_uri->path, 
                               r->parsed_uri->query ? "?" : "",
                               r->parsed_uri->query ? r->parsed_uri->query : "",
                               r->keepalive ? "Keep-Alive" : "Close",
                               r->parsed_uri->hostinfo,
                               cookies);
        r->rbuftype = POOL;
        r->rbufsize = strlen(r->rbuf);
        break;
    case HEAD:
        r->rbuf = apr_psprintf(r->pool, 
                               "HEAD %s%s%s HTTP/1.1" CRLF
                               "User-Agent: Flood/" FLOOD_VERSION CRLF
                               "Connection: %s" CRLF
                               "Host: %s" CRLF 
                               "%s" CRLF,
                               r->parsed_uri->path, 
                               r->parsed_uri->query ? "?" : "",
                               r->parsed_uri->query ? r->parsed_uri->query : "",
                               r->keepalive ? "Keep-Alive" : "Close",
                               r->parsed_uri->hostinfo,
                               cookies);
        r->rbuftype = POOL;
        r->rbufsize = strlen(r->rbuf);
        break;
    case POST:
        /* FIXME */
        r->rbuf = apr_psprintf(r->pool, 
                               "POST %s%s%s HTTP/1.1" CRLF
                               "User-Agent: Flood/" FLOOD_VERSION CRLF
                               "Connection: %s" CRLF
                               "Host: %s" CRLF
                               "Content-Length: %d" CRLF 
                               "Content-type: application/x-www-form-urlencoded" CRLF
                               "%s" CRLF
                               "%s",
                               r->parsed_uri->path, 
                               r->parsed_uri->query ? "?" : "",
                               r->parsed_uri->query ? r->parsed_uri->query : "",
                               r->keepalive ? "Keep-Alive" : "Close",
                               r->parsed_uri->hostinfo,
                               r->payloadsize,
                               cookies,
                               (char*)r->payload);
        r->rbuftype = POOL;
        r->rbufsize = strlen(r->rbuf);
        break;
    }

    return APR_SUCCESS;
}

apr_status_t round_robin_profile_init(profile_t **profile, config_t *config, const char *profile_name, apr_pool_t *pool)
{
    apr_status_t stat;
    int i;
    struct apr_xml_elem *root_elem, *profile_elem,
           *urllist_elem, *count_elem, *useurllist_elem, *e;
    round_robin_profile_t *p;
    char *xml_profile, *xml_urllist, *urllist_name;

    p = apr_pcalloc(pool, sizeof(round_robin_profile_t));
    p->pool = pool;

    /* yeah, yeah; calloc(), whatever...this is readability baby! */
    p->current_url = 0; /* start on the first URL */
    p->current_round = 0; /* start counting rounds at 0 */

    /* get the XML pathes to the profile and the urllist */
    xml_profile = apr_pstrdup(pool, XML_PROFILE);
    xml_urllist = apr_pstrdup(pool, XML_URLLIST);

    if ((stat = retrieve_root_xml_elem(&root_elem, config)) != APR_SUCCESS) {
        return stat;
    }

    /* retrieve our profile xml element */
    if ((stat = retrieve_xml_elem_with_childmatch(
             &profile_elem, root_elem,
             xml_profile, "name", profile_name)) != APR_SUCCESS)
        return stat;

    /* find the count */
    if ((stat = retrieve_xml_elem_child(
             &count_elem, profile_elem, XML_PROFILE_COUNT)) != APR_SUCCESS) {
        /* if it's missing, just default to 1 */
        p->execute_rounds = 1;
    } else {
        if (count_elem->first_cdata.first && count_elem->first_cdata.first->text) {
            p->execute_rounds = strtol(count_elem->first_cdata.first->text, NULL, 10);
            if (p->execute_rounds == LONG_MAX || p->execute_rounds == LONG_MIN)
                /* error, over/under-flow */
                return errno;
        } else {
            apr_file_printf(local_stderr,
                            "Profile '%s' has element <%s> with no value, assuming 1.\n",
                            profile_name, XML_PROFILE_COUNT);
            p->execute_rounds = 1;     
        }
    }

#ifdef PROFILE_DEBUG
    apr_file_printf(local_stdout,
                    "Profile '%s' will be run %d times.\n", profile_name, p->execute_rounds);
#endif /* PROFILE_DEBUG */

    /* find out what the name of our urllist is */
    if ((stat = retrieve_xml_elem_child(
             &useurllist_elem, profile_elem, XML_PROFILE_USEURLLIST)) != APR_SUCCESS) {
        /* useurllist is a required parameter, error */
        apr_file_printf(local_stderr,
                        "Profile '%s' has no <%s> parameter.\n",
                        profile_name, XML_PROFILE_USEURLLIST);
        return APR_EGENERAL;
    } else {
        urllist_name = apr_pstrdup(pool, useurllist_elem->first_cdata.first->text);
    }

    /* retrieve our urllist xml element */
    if ((stat = retrieve_xml_elem_with_childmatch(
             &urllist_elem, root_elem,
             xml_urllist, XML_URLLIST_NAME, urllist_name)) != APR_SUCCESS)
        return stat;

    /* find the urllist for this profile, put 'em in this list */
    if ((p->urls = count_xml_elem_child(urllist_elem, XML_URLLIST_URL)) <= 0) {
        apr_file_printf(local_stderr, "Urllist '%s' doesn't have any urls!\n", urllist_name);
        return APR_EGENERAL;
    }
    p->url = apr_pcalloc(p->pool, sizeof(url_t) * (p->urls + 1));

    i = 0;
    for (e = urllist_elem->first_child; e; e = e->next) {
        if (strncasecmp(e->name, XML_URLLIST_URL, FLOOD_STRLEN_MAX) == 0) {
            /* Do we need strdup? */
            p->url[i].url = apr_pstrdup(pool, e->first_cdata.first->text);
            if (e->attr)
            {
                apr_xml_attr *attr = e->attr;
                while (attr)
                {
                    if (strncasecmp(attr->name, XML_URLLIST_METHOD, 
                                FLOOD_STRLEN_MAX) == 0) {
                        if (strncasecmp(attr->value, XML_URLLIST_METHOD_POST, 4) == 0)
                            p->url[i].method = POST;
                        else if (strncasecmp(attr->value, XML_URLLIST_METHOD_HEAD, 4) == 0)
                            p->url[i].method = HEAD;
                        else if (strncasecmp(attr->value, XML_URLLIST_METHOD_GET, 3) == 0)
                            p->url[i].method = GET;
                        else {
                            apr_file_printf(local_stderr, "Attribute %s has invalid value %s.\n",
                                            XML_URLLIST_METHOD, attr->value);
                            return APR_EGENERAL;
                        }
                    }
                    else if (strncasecmp(attr->name, XML_URLLIST_PAYLOAD, 
                                     FLOOD_STRLEN_MAX) == 0) {
                        p->url[i].payload = (char*)attr->value;
                    }
                    else if (strncasecmp(attr->name, XML_URLLIST_DELAY,
                                         FLOOD_STRLEN_MAX) == 0) {
                        char *endptr;
                        p->url[i].delay = strtoll(attr->value, &endptr, 10);
                        if (*endptr != '\0')
                        {
                            apr_file_printf(local_stderr, 
                                        "Attribute %s has invalid value %s.\n",
                                        XML_URLLIST_DELAY, attr->value);
                            return APR_EGENERAL;
                        }
                        p->url[i].delay *= APR_USEC_PER_SEC;
                    }
                    attr = attr->next;
                }
            }
            else
            {
                p->url[i].method = GET;
                p->url[i].payload = NULL;
            }

            i++;
        }
    }

    *profile = p;

    return APR_SUCCESS;
}

apr_status_t round_robin_get_next_url(request_t **request, profile_t *profile)
{
    round_robin_profile_t *rp;
    request_t *r;

    rp = (round_robin_profile_t*)profile;

    /* FIXME: precompute request_t in profile_init */
    r = apr_pcalloc(rp->pool, sizeof(request_t));
    r->pool = rp->pool;

    r->uri = rp->url[rp->current_url].url;
    r->method = rp->url[rp->current_url].method;

    /* We're created by calloc, so no need to set payload to be null or
     * payloadsize to be 0. 
     */
    if (rp->url[rp->current_url].payload)
    {
        r->payload = rp->url[rp->current_url].payload;
        r->payloadsize = strlen(rp->url[rp->current_url].payload);
    }

    /* If they want a sleep, do it now. */
    if (rp->url[rp->current_url].delay)
        apr_sleep(rp->url[rp->current_url].delay);

    rp->current_url++;

    r->parsed_uri = apr_pcalloc(rp->pool, sizeof(apr_uri_components));

    apr_uri_parse_components(rp->pool, r->uri, r->parsed_uri);
    if (!r->parsed_uri->port)
    {
        r->parsed_uri->port = 
                         apr_uri_default_port_for_scheme(r->parsed_uri->scheme);
    }
    if (!r->parsed_uri->path) /* If / is not there, be nice.  */
        r->parsed_uri->path = "/";

    /* Adjust counters for profile */
    if (rp->current_url >= rp->urls) {
        rp->current_url = 0;

        /* Loop cond tells us when to stop. */
        rp->current_round++;
    }

#ifdef PROFILE_DEBUG
    apr_file_printf(local_stdout, "Generating request to: %s\n", r->uri);
#endif /* PROFILE_DEBUG */

    *request = r;

    return APR_SUCCESS;
}

apr_status_t round_robin_postprocess(profile_t *profile,
                                     request_t *req,
                                     response_t *resp)
{
    round_robin_profile_t *rp;
    char *cookieheader, *cookievalue, *cookieend;

    rp = (round_robin_profile_t*)profile;

    /* FIXME: This algorithm sucks.  I need to be shot for writing such 
     * atrocious code.  Grr.  */
    cookieheader = strstr(resp->rbuf, "Set-Cookie: ");
    if (cookieheader)
    {
        /* Point to the value */
        cookieheader += 12;
        cookievalue = (char*) memchr(cookieheader, '=', 
                      resp->rbufsize - (int)(cookieheader - (int)(resp->rbuf)));
        if (cookievalue)
        {
            cookie_t * cookie = apr_pcalloc(rp->pool, sizeof(cookie_t));

            *cookievalue = '\0';
            cookie->name = apr_pstrdup(rp->pool, cookieheader);

            cookieheader = ++cookievalue;
            cookieend = (char*) memchr(cookieheader, '\r', 
                      resp->rbufsize - (int)(cookieheader - (int)(resp->rbuf)));
            cookievalue = (char*) memchr(cookieheader, ';', 
                                  cookieend - cookieheader);
            if (cookievalue)
                *cookievalue = '\0';
            else
                *cookieend = '\0';
            
            cookie->value = apr_pstrdup(rp->pool, cookieheader);
            cookie->next = rp->cookie;
            rp->cookie = cookie;
        }
    }
    return APR_SUCCESS;
}

apr_status_t verify_200(int *verified, profile_t *profile, request_t *req, response_t *resp)
{
    round_robin_profile_t *rp;
    int res;

    rp = (round_robin_profile_t*) profile;

    res = memcmp(resp->rbuf, "HTTP/1.1 2", 10);

    if (!res)
        *verified = FLOOD_VALID;
    else if (memcmp(resp->rbuf + 9, "3", 1) == 0) /* Accept 3xx as okay. */
        *verified = FLOOD_VALID;
    else
        *verified = FLOOD_INVALID;

    return APR_SUCCESS;
}

int round_robin_loop_condition(profile_t *profile)
{
    round_robin_profile_t *rp;

    rp = (round_robin_profile_t*)profile;

#ifdef PROFILE_DEBUG
    apr_file_printf(local_stdout, "Round %d of %d, %s.\n",
                    rp->current_round, rp->execute_rounds,
                    (rp->current_round < rp->execute_rounds ? "Continuing" : "Finished"));
#endif /* PROFILE_DEBUG */

    return (rp->current_round < rp->execute_rounds);
}

apr_status_t round_robin_profile_destroy(profile_t *profile)
{
    /* FIXME: free() the memory used by this profile, or reset() the pool
     * (or whatever semantics apr uses, I dunno...) -aaron */
    return APR_SUCCESS;
}

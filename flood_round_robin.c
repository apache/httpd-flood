/* Copyright 2001-2004 The Apache Software Foundation
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
#include <apr_network_io.h>
#include <apr_strings.h>
#include <apr_uri.h>
#include <apr_lib.h>
#include <apr_hash.h>
#include <apr_base64.h>
#include <apr_poll.h>
#include <apr_thread_proc.h>
#include <apr_errno.h>

#if APR_HAVE_STRINGS_H
#include <strings.h>    /* strncasecmp */
#endif
#if APR_HAVE_STRING_H
#include <string.h>    /* strncasecmp */
#endif
#if APR_HAVE_STDLIB_H
#include <stdlib.h>     /* strtol */
#endif
#if APR_HAVE_UNISTD_H
#include <unistd.h>      /* For pause */
#endif
#if APR_HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if APR_HAVE_LIMITS_H
#include <limits.h>
#endif
#include <assert.h>
#ifdef FLOOD_USE_PCRE
#include "flood_pcre.h"
#else
#include "regex.h"
#endif

#include "config.h"
#include "flood_net.h"
#include "flood_round_robin.h"
#include "flood_profile.h"

/* On FreeBSD, the return of regexec() is 0 or REG_NOMATCH, and REG_OK is undefined */
#ifndef REG_OK
#define REG_OK 0
#endif

extern apr_file_t *local_stdout;
extern apr_file_t *local_stderr;

typedef enum {
    EPE_EXPAND,
    EPE_EXPAND_SET,
    EPE_PASSTHROUGH
} expand_param_e;

typedef struct {
    char *url;
    method_e method;
    const char *method_string;
    char *payload;
    char *contenttype;
    char *extra_headers;
    apr_int64_t predelay;
    apr_int64_t predelayprecision;
    apr_int64_t postdelay;
    apr_int64_t postdelayprecision;

    char *payloadtemplate;
    char *requesttemplate;
    char *responsetemplate;
    char *responsescript;
    char *responsename;
    int responselen;
    char *user;
    char *password;
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
    char *baseurl;
    apr_uri_t *proxy_url;

    cookie_t *cookie;

    apr_hash_t *state;

    int current_round;
    int current_url;

} round_robin_profile_t;

static char *handle_param_string(round_robin_profile_t *rp, char *template, 
                                 expand_param_e set)
{
    char *cpy, *cur, *prev, *data, *returnValue, *pattern;
    int size, matchsize;
    regex_t re;
    regmatch_t match[2];
         
    prev = template;
    returnValue = NULL;

    pattern = "\\$\\{([^\\}]+)\\}";
    regcomp(&re, pattern, REG_EXTENDED);
    cur = template;
    while (regexec(&re, cur, 2, match, 0) == REG_OK)
    {
        /* We must backup over the ${ characters. */
        size = match[1].rm_so - 2;
        if (size++)
        {
            cpy = apr_palloc(rp->pool, size);
            apr_cpystrn(cpy, cur, size);
        }
        else
            cpy = NULL;

        if (*(cur+match[1].rm_so) == '=')
        {
            if (set)
            {
                /* We need to assign it a random value. */
#if FLOOD_USE_RAND
                data = apr_psprintf(rp->pool, "%d", rand());
#elif FLOOD_USE_RAND48
                data = apr_psprintf(rp->pool, "%ld", lrand48());
#elif FLOOD_USE_RANDOM
                data = apr_psprintf(rp->pool, "%ld", (long)random());
#endif
                matchsize = match[1].rm_eo - match[1].rm_so - 1;
                apr_hash_set(rp->state, cur+match[1].rm_so+1, matchsize, data);
            }
            else
                data = NULL;
        }
        else
        {
            matchsize = match[1].rm_eo - match[1].rm_so;
            data = apr_hash_get(rp->state, cur+match[1].rm_so, matchsize);
        }

        /* If there is no data, place the original string back. */
        if (!data) {
            data = apr_psprintf(rp->pool, "${%s}", 
                                apr_pstrmemdup(rp->pool, cur+match[1].rm_so,
                                match[1].rm_eo - match[1].rm_so));
        }

        if (!returnValue)
        {
            if (cpy)
                returnValue = apr_pstrcat(rp->pool, cpy, data, NULL);
            else
                returnValue = apr_pstrdup(rp->pool, data);
        }
        else
        {
            if (cpy)
                returnValue = apr_pstrcat(rp->pool, returnValue, cpy, data, 
                                          NULL);
            else
                returnValue = apr_pstrcat(rp->pool, returnValue, data, NULL);
            
        }

        /* Skip over the trailing } */
        cur += match[1].rm_eo + 1;
    }

    if (!returnValue)
        returnValue = apr_pstrdup(rp->pool, cur);
    else
        returnValue = apr_pstrcat(rp->pool, returnValue, cur, NULL);

    regfree(&re);
    return returnValue;
}

static char *expand_param_string(round_robin_profile_t *rp, char *template)
{
    return handle_param_string(rp, template, EPE_EXPAND);
}

static char *parse_param_string(round_robin_profile_t *rp, char *template)
{
    return handle_param_string(rp, template, EPE_EXPAND_SET);
}

/* Construct a request */
apr_status_t round_robin_create_req(profile_t *profile, request_t *r)
{
    round_robin_profile_t *p;
    char *cookies, *path;
    char *enc_credtls, *credtls, *authz_hdr = NULL, *extra_hdr = NULL;
    cookie_t *cook;
   
    p = (round_robin_profile_t*)profile; 

    /* Do we want to save the entire response? */
    r->wantresponse = p->url[p->current_url].responsetemplate ? 1 : 0;

    /* FIXME: This algorithm sucks. */
    if (p->cookie)
    {
        cookies = apr_pstrdup(p->pool, "Cookie: ");
        cook = p->cookie;
        while (cook)
        {
            if (cook != p->cookie)
                cookies = apr_pstrcat(p->pool, cookies, ";", NULL);

            cookies = apr_pstrcat(p->pool, cookies, cook->name, "=", 
                                  cook->value, NULL);
            cook = cook->next; 
        }
        cookies = apr_pstrcat(p->pool, cookies, CRLF, NULL);
    }
    else
        cookies = "";

    if (p->url[p->current_url].user) {
        if (!p->url[p->current_url].password) {
            apr_file_printf(local_stderr,
                            "missing password for user '%s'\n",
                            p->url[p->current_url].user);
            return APR_EGENERAL;
        } else {
            int credlen;
            credtls = apr_pstrcat(r->pool, p->url[p->current_url].user,
                                  ":", p->url[p->current_url].password, NULL);
            credlen = strlen(credtls);
            enc_credtls = (char *) apr_palloc(r->pool,
                                              apr_base64_encode_len(credlen) + 1);
            apr_base64_encode(enc_credtls, credtls, credlen);
            authz_hdr = apr_pstrcat(r->pool, "Authorization: Basic ",
                                    enc_credtls, CRLF, NULL);
        }
    }

    if (p->url[p->current_url].extra_headers) {
        extra_hdr = p->url[p->current_url].extra_headers;
    }

    if (p->proxy_url != NULL) {
        path = apr_pstrcat(r->pool, r->parsed_uri->scheme, "://",
                                    r->parsed_uri->hostinfo,
                                    r->parsed_uri->path, NULL);
    }
    else {
        path = r->parsed_uri->path;
    }

    switch (r->method)
    {
    case GET:
    case HEAD:
        r->rbuf = apr_psprintf(r->pool, 
                               "%s %s%s%s HTTP/1.1" CRLF
                               "User-Agent: Flood/" FLOOD_VERSION CRLF
                               "Connection: %s" CRLF
                               "Host: %s" CRLF 
                               "%s" /* Extra headers */
                               "%s" /* Authz */
                               "%s" /* Cookies */
                               CRLF, /* End of HTTP request headers */
                               p->url[p->current_url].method_string,
                               path,
                               r->parsed_uri->query ? "?" : "",
                               r->parsed_uri->query ? r->parsed_uri->query : "",
                               r->keepalive ? "Keep-Alive" : "Close",
                               r->parsed_uri->hostinfo,
                               extra_hdr ? extra_hdr : "",
                               authz_hdr ? authz_hdr : "",
                               cookies);
        r->rbuftype = POOL;
        r->rbufsize = strlen(r->rbuf);
        break;
    case POST:
    case OTHER:
        if (r->payload) {
            r->rbuf = apr_psprintf(r->pool, 
                                   "%s %s%s%s HTTP/1.1" CRLF
                                   "User-Agent: Flood/" FLOOD_VERSION CRLF
                                   "Connection: %s" CRLF
                                   "Host: %s" CRLF
                                   "Content-Length: %d" CRLF 
                                   "Content-Type: %s" CRLF 
                                   "%s" /* Extra headers */
                                   "%s" /* Authz */
                                   "%s" /* Cookies */
                                   CRLF /* End of HTTP request headers */
                                   "%s" /* Body */,
                                   p->url[p->current_url].method_string,
                                   path, 
                                   r->parsed_uri->query ? "?" : "",
                                   r->parsed_uri->query ?
                                       r->parsed_uri->query : "",
                                   r->keepalive ? "Keep-Alive" : "Close",
                                   r->parsed_uri->hostinfo,
                                   r->payloadsize,
                                   r->contenttype ? r->contenttype :
                                       "application/x-www-form-urlencoded", 
                                   extra_hdr ? extra_hdr : "",
                                   authz_hdr ? authz_hdr : "",
                                   cookies,
                                   (char*)r->payload);
        } else { /* There is no payload, but it's still a POST */
            r->rbuf = apr_psprintf(r->pool, 
                                   "%s %s%s%s HTTP/1.1" CRLF
                                   "User-Agent: Flood/" FLOOD_VERSION CRLF
                                   "Connection: %s" CRLF
                                   "Host: %s" CRLF
                                   "%s" /* Extra headers */
                                   "%s" /* Authz */
                                   "%s" /* Cookies */
                                   CRLF /* End of HTTP request headers */
                                   "" /* No body */,
                                   p->url[p->current_url].method_string,
                                   path, 
                                   r->parsed_uri->query ? "?" : "",
                                   r->parsed_uri->query ? r->parsed_uri->query : "",
                                   r->keepalive ? "Keep-Alive" : "Close",
                                   r->parsed_uri->hostinfo,
                                   extra_hdr ? extra_hdr : "",
                                   authz_hdr ? authz_hdr : "",
                                   cookies);
        }
        r->rbuftype = POOL;
        r->rbufsize = strlen(r->rbuf);
        break;
    }

    return APR_SUCCESS;
}

static apr_status_t parse_xml_url_info(apr_xml_elem *e, url_t *url,
                                       apr_pool_t *pool)
{
    /* Grab the url from the text section. */
    if (e->first_cdata.first && e->first_cdata.first->text)
    {
        if (e->first_cdata.first->next)
        {
            apr_text *t;
            t = e->first_cdata.first; 
            url->url = apr_pstrdup(pool, t->text);
            while ((t = t->next))
            {
                url->url = apr_pstrcat(pool, url->url, t->text, NULL);
            }
        }
        else {
            url->url = apr_pstrdup(pool, e->first_cdata.first->text);
        }
    }

    /* Parse any attributes. */
    if (e->attr)
    {
        apr_xml_attr *attr = e->attr;
        while (attr)
        {
            if (strncasecmp(attr->name, XML_URLLIST_METHOD, 
                            FLOOD_STRLEN_MAX) == 0) {
                if (strncasecmp(attr->value, XML_URLLIST_METHOD_POST, 4) == 0) {
                    url->method = POST;
                    url->method_string = "POST";
                }
                else if (strncasecmp(attr->value, XML_URLLIST_METHOD_HEAD,
                                     4) == 0)
                {
                    url->method = HEAD;
                    url->method_string = "HEAD";
                }
                else if (strncasecmp(attr->value, XML_URLLIST_METHOD_GET,
                                     3) == 0) {
                    url->method = GET;
                    url->method_string = "GET";
                }
                else {
                    url->method = OTHER;
                    url->method_string = apr_pstrdup(pool, attr->value);
                }
            }
            else if (strncasecmp(attr->name, XML_URLLIST_PAYLOAD, 
                                 FLOOD_STRLEN_MAX) == 0) {
                url->payload = (char*)attr->value;
            }
            else if (strncasecmp(attr->name, XML_URLLIST_PAYLOAD_FILE,
                                 FLOOD_STRLEN_MAX) == 0) {
                apr_status_t status;
                const char *fname;
                apr_finfo_t finfo;
                apr_file_t *file;
                apr_size_t len;

                fname = attr->value;

                status = apr_stat(&finfo, fname, APR_FINFO_MIN, pool);
                if (status != APR_SUCCESS)
                    return status;
               
                status = apr_file_open(&file, fname, APR_READ, APR_OS_DEFAULT,
                                       pool);
                if (status != APR_SUCCESS)
                    return status;

                url->payload = apr_palloc(pool, finfo.size + 1);

                status = apr_file_read_full(file, url->payload, finfo.size,
                                            &len);
                if (len != finfo.size)
                    return status;

                apr_file_close(file);
            }
            else if (strncasecmp(attr->name,XML_URLLIST_CONTENT_TYPE,
                                 FLOOD_STRLEN_MAX) == 0) {
                url->contenttype = (char*)attr->value;
            }
            else if (strncasecmp(attr->name,XML_URLLIST_EXTRA_HEADERS,
                                 FLOOD_STRLEN_MAX) == 0) {
                char *last, *header;

                header = apr_strtok((char*)attr->value, ";", &last);
                while (header) {
                    apr_collapse_spaces(header, header);
                    if (url->extra_headers) {
                        url->extra_headers = apr_pstrcat(pool,
                                                         url->extra_headers,
                                                         header, CRLF, NULL);
                    }
                    else {
                        url->extra_headers = apr_pstrcat(pool,
                                                         header, CRLF, NULL);
                    }

                    header = apr_strtok(NULL, ";", &last);
                }
            }
            else if (strncasecmp(attr->name, XML_URLLIST_PREDELAY,
                                 FLOOD_STRLEN_MAX) == 0) {
                char *endptr;
                url->predelay = strtoll(attr->value, &endptr, 10);
                if (*endptr != '\0')
                {
                    apr_file_printf(local_stderr, 
                                    "Attribute %s has invalid value %s.\n",
                                    XML_URLLIST_PREDELAY, attr->value);
                    return APR_EGENERAL;
                }
                url->predelay *= APR_USEC_PER_SEC;
            }
            else if (strncasecmp(attr->name, XML_URLLIST_PREDELAYPRECISION,
                                 FLOOD_STRLEN_MAX) == 0) {
                char *endptr;
                url->predelayprecision = strtoll(attr->value, &endptr, 10);
                if (*endptr != '\0')
                {
                    apr_file_printf(local_stderr,
                                    "Attribute %s has invalid value %s.\n",
                                    XML_URLLIST_PREDELAYPRECISION, attr->value);
                    return APR_EGENERAL;
                }
                url->predelayprecision *= APR_USEC_PER_SEC;
            }
            else if (strncasecmp(attr->name, XML_URLLIST_POSTDELAY,
                                 FLOOD_STRLEN_MAX) == 0) {
                char *endptr;
                url->postdelay = strtoll(attr->value, &endptr, 10);
                if (*endptr != '\0')
                {
                    apr_file_printf(local_stderr, 
                                    "Attribute %s has invalid value %s.\n",
                                    XML_URLLIST_POSTDELAY, attr->value);
                    return APR_EGENERAL;
                }
                url->postdelay *= APR_USEC_PER_SEC;
            }
            else if (strncasecmp(attr->name, XML_URLLIST_POSTDELAYPRECISION,
                                 FLOOD_STRLEN_MAX) == 0) {
                char *endptr;
                url->postdelayprecision = strtoll(attr->value, &endptr, 10);
                if (*endptr != '\0')
                {
                    apr_file_printf(local_stderr,
                                    "Attribute %s has invalid value %s.\n",
                                    XML_URLLIST_POSTDELAYPRECISION, attr->value);
                    return APR_EGENERAL;
                }
                url->postdelayprecision *= APR_USEC_PER_SEC;
            }
            else if (strncasecmp(attr->name, 
                                 XML_URLLIST_PAYLOAD_TEMPLATE, 
                                 FLOOD_STRLEN_MAX) == 0) {
                url->payloadtemplate = (char*)attr->value;
            }
            else if (strncasecmp(attr->name, 
                                 XML_URLLIST_REQUEST_TEMPLATE, 
                                 FLOOD_STRLEN_MAX) == 0) {
                url->requesttemplate = (char*)attr->value;
            }
            else if (strncasecmp(attr->name, 
                                 XML_URLLIST_RESPONSE_TEMPLATE, 
                                 FLOOD_STRLEN_MAX) == 0) {
                url->responsetemplate = (char*)attr->value;
            }
            else if (strncasecmp(attr->name, 
                                 XML_URLLIST_RESPONSE_SCRIPT, 
                                 FLOOD_STRLEN_MAX) == 0) {
                url->responsescript = (char*)attr->value;
            }
            else if (strncasecmp(attr->name, 
                                 XML_URLLIST_RESPONSE_NAME,
                                 FLOOD_STRLEN_MAX) == 0) {
                url->responsename = (char*)attr->value;
                url->responselen = strlen((char*)attr->value);
            }
            else if (strncasecmp(attr->name, 
                                 XML_URLLIST_USER,
                                 FLOOD_STRLEN_MAX) == 0) {
                url->user = (char*)attr->value;
            }
            else if (strncasecmp(attr->name, 
                                 XML_URLLIST_PASSWORD,
                                 FLOOD_STRLEN_MAX) == 0) {
                url->password = (char*)attr->value;
            }
            attr = attr->next;
        }
    }
    else
    {
        url->method = GET;
        url->method_string = "GET";
        url->payload = NULL;
        url->contenttype = NULL;
    }

    return APR_SUCCESS;
}
        
static apr_status_t parse_xml_seq_info(apr_xml_elem *e,
                                       round_robin_profile_t *p,
                                       apr_pool_t *pool)
{
    char *seqname, **seqlist;
    int seqnamelen, seqcount, curseq;
    struct apr_xml_elem *child_url_elem;
    apr_status_t rv;

    if (e->attr) {
        apr_xml_attr *attr = e->attr;
        while (attr) {
            if (strncasecmp(attr->name, XML_URLLIST_SEQUENCE_NAME,
                            FLOOD_STRLEN_MAX) == 0) {
                seqname = (char*)attr->value;
                seqnamelen = strlen(seqname);
            }             
            else if (strncasecmp(attr->name, 
                         XML_URLLIST_SEQUENCE_LIST,
                         FLOOD_STRLEN_MAX) == 0) {
                /* FIXME: ap_getword needs to be in apr-util! */
                char *end, *cur;
                int count = 1, num = 0;
                end = (char*)attr->value;
                while (*end && (end = strchr(end, ','))) {
                    count++;
                    end++;
                } 
                seqlist = apr_palloc(pool, sizeof(char*) * count);
                seqcount = count;

                cur = (char*)attr->value;
                end = strchr(cur, ',');
                for (num = 0; num < count; num++) {
                    while (apr_isspace(*cur)) { 
                        cur++;
                    }
                    if (end) {
                        seqlist[num] = apr_pstrmemdup(pool, cur,
                                                      end - cur);
                        cur = ++end;
                        end = strchr(cur, ',');
                    }
                    else {
                        seqlist[num] = apr_pstrdup(pool, cur);
                    }
                }
            } 
            attr = attr->next; 
        }
    }             
    for (curseq = 0; curseq < seqcount; curseq++) {
        apr_hash_set(p->state, seqname, seqnamelen, seqlist[curseq]);
        for (child_url_elem = e->first_child; child_url_elem;
             child_url_elem = child_url_elem->next) {
            if (strncasecmp(child_url_elem->name, XML_URLLIST_SEQUENCE,
                            FLOOD_STRLEN_MAX) == 0) {
                rv = parse_xml_seq_info(child_url_elem, p, pool);
                if (rv != APR_SUCCESS) {
                    return rv;
                }
            }
            else if (strncasecmp(child_url_elem->name, XML_URLLIST_URL,
                                 FLOOD_STRLEN_MAX) == 0) {
                rv = parse_xml_url_info(child_url_elem,
                                        &p->url[p->current_url],
                                        pool);
                if (rv != APR_SUCCESS) {
                    return rv;
                }
                /* Expand them. */
                if (p->url[p->current_url].payloadtemplate) {
                    p->url[p->current_url].payloadtemplate = 
                        handle_param_string(p,
                                        p->url[p->current_url].payloadtemplate,
                                        EPE_PASSTHROUGH);
                }
                if (p->url[p->current_url].requesttemplate) {
                    p->url[p->current_url].requesttemplate = 
                        handle_param_string(p,
                                        p->url[p->current_url].requesttemplate,
                                        EPE_PASSTHROUGH);
                }
                if (p->url[p->current_url].responsetemplate) {
                    p->url[p->current_url].responsetemplate = 
                        handle_param_string(p,
                                        p->url[p->current_url].responsetemplate,
                                        EPE_PASSTHROUGH);
                }
                p->current_url++;
            }
        }
    }
    return APR_SUCCESS;
}

static int count_xml_seq_child(apr_xml_elem *urllist_elem)
{
    struct apr_xml_elem *e;
    int items = 0;

    for (e = urllist_elem->first_child; e; e = e->next) {
        if (strncasecmp(e->name, XML_URLLIST_SEQUENCE, FLOOD_STRLEN_MAX) == 0) {
            int children_urls, list_count;
            list_count = 0;
            if (e->attr) {
                apr_xml_attr *attr = e->attr;
                while (attr) {
                    if (strncasecmp(attr->name, 
                                    XML_URLLIST_SEQUENCE_LIST,
                                    FLOOD_STRLEN_MAX) == 0) {
                        char *end = (char*)attr->value;
                        list_count++;
                        while (*end && (end = strchr(end, ','))) {
                            list_count++;
                            end++;
                        }
                    }
                    attr = attr->next;
                }
            }
            if (!list_count) {
                apr_file_printf(local_stderr,
                                "Sequence doesn't have any items!\n");
                return 0;
            }
            children_urls = count_xml_seq_child(e);
            children_urls += count_xml_elem_child(e, XML_URLLIST_URL);
            items += list_count * children_urls;
        }
    }
    return items;
}

apr_status_t round_robin_profile_init(profile_t **profile,
                                      config_t *config,
                                      const char *profile_name,
                                      apr_pool_t *pool)
{
    apr_status_t rv;
    int i;
    struct apr_xml_elem *root_elem, *profile_elem,
           *urllist_elem, *count_elem, *useurllist_elem, *baseurl_elem,
           *proxyurl_elem, *e;
    round_robin_profile_t *p;
    char *xml_profile, *xml_urllist, *urllist_name;

    p = apr_pcalloc(pool, sizeof(round_robin_profile_t));
    p->pool = pool;

    /* yeah, yeah; calloc(), whatever...this is readability baby! */
    p->current_url = 0; /* start on the first URL */
    p->current_round = 0; /* start counting rounds at 0 */
    p->state = apr_hash_make(pool);

    /* get the XML pathes to the profile and the urllist */
    xml_profile = apr_pstrdup(pool, XML_PROFILE);
    xml_urllist = apr_pstrdup(pool, XML_URLLIST);

    if ((rv = retrieve_root_xml_elem(&root_elem, config)) != APR_SUCCESS) {
        return rv;
    }

    /* retrieve our profile xml element */
    if ((rv = retrieve_xml_elem_with_childmatch(
             &profile_elem, root_elem,
             xml_profile, "name", profile_name)) != APR_SUCCESS)
        return rv;

    /* find the count */
    if ((rv = retrieve_xml_elem_child(
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
    if ((rv = retrieve_xml_elem_child(
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
    if ((rv = retrieve_xml_elem_with_childmatch(
             &urllist_elem, root_elem,
             xml_urllist, XML_URLLIST_NAME, urllist_name)) != APR_SUCCESS)
        return rv;

    /* do we have base url? */
    if ((rv = retrieve_xml_elem_child(
             &baseurl_elem, urllist_elem, XML_URLLIST_BASE_URL)) == APR_SUCCESS) {
        /* yes we do */
        p->baseurl = apr_pstrdup(pool, baseurl_elem->first_cdata.first->text);
    } else {
        p->baseurl = NULL;
    }

    /* do we have proxy url? */
    if ((rv = retrieve_xml_elem_child(
             &proxyurl_elem, urllist_elem, XML_URLLIST_PROXY_URL)) == APR_SUCCESS) {
        /* yes we do */
        p->proxy_url = apr_pcalloc(p->pool, sizeof(apr_uri_t));
        apr_uri_parse(p->pool, proxyurl_elem->first_cdata.first->text,
                      p->proxy_url);
    } else {
        p->proxy_url = NULL;
    }

    p->urls = 0;
    /* Include sequences.  We'll expand them later. */
    p->urls = count_xml_seq_child(urllist_elem);
    /* find the urls for this profile, put 'em in this list */
    p->urls += count_xml_elem_child(urllist_elem, XML_URLLIST_URL);

    if (p->urls <= 0) {
        apr_file_printf(local_stderr, "Urllist '%s' doesn't have any urls!\n", urllist_name);
        return APR_EGENERAL;
    }
    p->url = apr_pcalloc(p->pool, sizeof(url_t) * (p->urls + 1));

    i = 0;
    for (e = urllist_elem->first_child; e; e = e->next) {
        if (strncasecmp(e->name, XML_URLLIST_SEQUENCE, FLOOD_STRLEN_MAX) == 0) {
            rv = parse_xml_seq_info(e, p, pool);
            if (rv != APR_SUCCESS) {
                return rv;
            }
        }
        if (strncasecmp(e->name, XML_URLLIST_URL, FLOOD_STRLEN_MAX) == 0) {
            rv = parse_xml_url_info(e, &p->url[p->current_url++], pool);
            if (rv != APR_SUCCESS) {
                return rv;
            }
        }
    }

    /* Reset this back to 0. */
    p->current_url = 0;

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

    if (rp->url[rp->current_url].requesttemplate)
    {
        r->uri = parse_param_string(rp, 
                                    rp->url[rp->current_url].requesttemplate);
    }
    else
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
    else if (rp->url[rp->current_url].payloadtemplate)
    {
        r->payload = parse_param_string(rp, 
                                    rp->url[rp->current_url].payloadtemplate);
        r->payloadsize = strlen(r->payload);
    }

    if (rp->url[rp->current_url].contenttype)
    {
        r->contenttype = parse_param_string(rp, rp->url[rp->current_url].contenttype);
        r->contenttypesize = strlen(r->contenttype);
    }

    /* If they want a sleep, do it now. */
    if (rp->url[rp->current_url].predelay) {
        apr_int64_t real_predelay = rp->url[rp->current_url].predelay;

        /* If the delay has a precision, adjust the
         * delay by some random fraction of the precision here */
        if (rp->url[rp->current_url].predelayprecision) {
            /* FIXME: this should be more portable, like apr_generate_random_bytes() */
            float factor = -1.0 + (2.0*rand()/(RAND_MAX+1.0));
            real_predelay += rp->url[rp->current_url].predelayprecision * factor;
        }

        /* we can only delay positive times, can't go back in time :( */
        if (real_predelay < 0)
            real_predelay = 0;

        /* only bother going to sleep if we generated a delay */
        if (real_predelay > 0)
            apr_sleep(real_predelay);

    }

    r->parsed_uri = apr_palloc(rp->pool, sizeof(apr_uri_t));

    if (rp->baseurl != NULL) {
        r->uri = apr_pstrcat(rp->pool, rp->baseurl, r->uri, NULL);
    }

    apr_uri_parse(rp->pool, r->uri, r->parsed_uri);
    if (r->parsed_uri->scheme == NULL || r->parsed_uri->hostname == NULL) {
        apr_file_printf(local_stderr, "Misformed URL '%s'\n", r->uri);
        exit (APR_EGENERAL);
    }
    if (r->parsed_uri->hostname[0] == '\0') {
        apr_file_printf(local_stderr,
                        "Misformed URL '%s' -- can't find valid hostname.\n",
                        r->uri);
        exit (APR_EGENERAL);
    }
    /* this schouldn't be hardcoded, but... :) */
    if (apr_strnatcmp (r->parsed_uri->scheme, "http") != APR_SUCCESS
        && apr_strnatcmp (r->parsed_uri->scheme, "https") != APR_SUCCESS) {
        apr_file_printf(local_stderr,
                        "Wrong URL scheme '%s' -- only 'http' and 'https' schemes are supported.\n",
                        r->parsed_uri->scheme);
        exit (APR_EGENERAL);
    }
    if (r->parsed_uri->user != NULL || r->parsed_uri->password != NULL) {
        apr_file_printf(local_stderr,
                        "Misformed URL -- auth data schould be outside URL -- please see docs.\n");
        exit (APR_EGENERAL);
    }
    if (!r->parsed_uri->port)
    {
        r->parsed_uri->port = 
                         apr_uri_port_of_scheme(r->parsed_uri->scheme);
    }
    if (!r->parsed_uri->path) /* If / is not there, be nice.  */
        r->parsed_uri->path = "/";

    r->parsed_proxy_uri = rp->proxy_url;

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

            ++cookievalue;
            cookie->name = apr_palloc(rp->pool, cookievalue - cookieheader);
            apr_cpystrn(cookie->name, cookieheader, cookievalue - cookieheader);

            cookieheader = cookievalue;
            cookieend = (char*) memchr(cookieheader, '\r', 
                      resp->rbufsize - (int)(cookieheader - (int)(resp->rbuf)));
            cookievalue = (char*) memchr(cookieheader, ';', 
                                  cookieend - cookieheader);
            if (!cookievalue)
                cookievalue = cookieend;
            
            ++cookievalue;
            
            cookie->value = apr_palloc(rp->pool, cookievalue - cookieheader);
            apr_cpystrn(cookie->value, cookieheader, 
                        cookievalue - cookieheader);
            cookie->next = rp->cookie;
            rp->cookie = cookie;
        }
    }
    if (rp->url[rp->current_url].responsetemplate)
    {
        int status, size;
        char *expanded, *newValue;
        regmatch_t match[10];
        regex_t re;

        expanded = expand_param_string(rp, 
                                    rp->url[rp->current_url].responsetemplate);
        regcomp(&re, expanded, REG_EXTENDED);
        status = regexec(&re, resp->rbuf, 10, match, 0);

        if (status != REG_OK) {
            apr_file_printf(local_stderr,
                            "Regular expression match failed (%s)\n",
                            rp->url[rp->current_url].responsetemplate);
            return APR_EGENERAL;
        }

        size = match[1].rm_eo - match[1].rm_so + 1;
        newValue = apr_palloc(rp->pool, size);
        apr_cpystrn(newValue, resp->rbuf + match[1].rm_so, size);
        apr_hash_set(rp->state, rp->url[rp->current_url].responsename,
                     rp->url[rp->current_url].responselen, newValue);
        regfree(&re);
    }
    if (rp->url[rp->current_url].responsescript)
    {
        int exitcode = 0;
        apr_status_t rv;
        apr_proc_t *proc;
        apr_pollfd_t pipeout;
        apr_pollset_t *pollset;
        apr_procattr_t *procattr;
        apr_size_t nbytes, wbytes;
        char buf[255];

        char **args;
        const char *progname;
        

        if ((rv = apr_procattr_create(&procattr, rp->pool)) != APR_SUCCESS) {
            apr_file_printf(local_stderr,
                            "apr_procattr_create failed for '%s': %s\n",
                            rp->url[rp->current_url].responsescript,
                            apr_strerror(rv, buf, sizeof(buf)));
            return rv;
        }

        if ((rv = apr_procattr_io_set(procattr, APR_FULL_BLOCK, APR_NO_PIPE,
                                      APR_NO_PIPE)) != APR_SUCCESS) {
            apr_file_printf(local_stderr,
                            "apr_procattr_io_set failed for '%s': %s\n",
                            rp->url[rp->current_url].responsescript,
                            apr_strerror(rv, buf, sizeof(buf)));
            return rv;
        }

        if ((rv = apr_procattr_error_check_set(procattr, 1)) != APR_SUCCESS) {
            apr_file_printf(local_stderr,
                            "apr_procattr_error_check_set failed "
                            "for '%s': %s\n",
                            rp->url[rp->current_url].responsescript,
                            apr_strerror(rv, buf, sizeof(buf)));
            return rv;
        }

        apr_tokenize_to_argv(rp->url[rp->current_url].responsescript, &args,
                                rp->pool);
        progname = apr_pstrdup(rp->pool, args[0]);

        proc = (apr_proc_t *)apr_pcalloc(rp->pool, sizeof(*proc));

        /* create process */
        if ((rv = apr_proc_create(proc, progname, (const char * const *)args,
                                  NULL, procattr, rp->pool)) != APR_SUCCESS) {
            apr_file_printf(local_stderr,
                            "Can't spawn postprocess script '%s': %s\n",
                            rp->url[rp->current_url].responsescript,
                            apr_strerror(rv, buf, sizeof(buf)));
            return rv;
        }

        if ((rv = apr_file_pipe_timeout_set(proc->in, apr_time_from_sec(10)))
                                    != APR_SUCCESS) {
            apr_file_printf(local_stderr,
                            "apr_file_pipe_timeout_set failed for '%s': %s\n",
                            rp->url[rp->current_url].responsescript,
                            apr_strerror(rv, buf, sizeof(buf)));
            return rv;
        }

        apr_pollset_create(&pollset, 1, rp->pool, 0);

        pipeout.desc_type = APR_POLL_FILE;
        pipeout.reqevents = APR_POLLOUT;
        pipeout.desc.f = proc->in;
        pipeout.client_data = NULL;

        apr_pollset_add(pollset, &pipeout);

        wbytes = 0;
        nbytes = strlen(resp->rbuf);

        while (wbytes < nbytes) {

            int bytes;
            apr_int32_t nrdes;
            const apr_pollfd_t *ardes = NULL;
            const apr_pollfd_t *rdes;

            if ((rv = apr_pollset_poll(pollset, apr_time_from_sec(10),
                                       &nrdes, &ardes)) != APR_SUCCESS) {
                apr_file_printf(local_stderr,
                                "error writing data to script '%s': %s\n",
                                rp->url[rp->current_url].responsescript,
                                apr_strerror(rv, buf, sizeof(buf)));
                return rv;
            }

            /* there can be only one descriptor... */
            rdes = &(ardes[0]);

            bytes = nbytes;
            apr_file_write(rdes->desc.f, resp->rbuf, &bytes);

            wbytes += bytes;

        }

        apr_pollset_remove(pollset, &pipeout);
        apr_file_close(proc->in);


        if ((rv = apr_proc_wait(proc, &exitcode, NULL, APR_WAIT))
                                                    != APR_CHILD_DONE) {
            apr_file_printf(local_stderr,
                            "apr_proc_wait failed for '%s': %s\n",
                            rp->url[rp->current_url].responsescript,
                            apr_strerror(rv, buf, sizeof(buf)));
            return rv;
        }

        if (exitcode != 0) {
            apr_file_printf(local_stderr,
                            "Postprocess script '%s' failed, exit code '%i'\n",
                            rp->url[rp->current_url].responsescript, exitcode);
            return APR_EGENERAL;
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
    else if (memcmp(resp->rbuf, "HTTP/1.0 2", 10) == 0) /* HTTP/1.0 is ok. */
        *verified = FLOOD_VALID;
    else
        *verified = FLOOD_INVALID;

    return APR_SUCCESS;
}

apr_status_t verify_status_code(int *verified, profile_t *profile,
                                request_t *req, response_t *resp)
{
    const char delimiter = ' ';
    char *state, *protocol, *scode;

    protocol = apr_strtok(resp->rbuf, &delimiter, &state);
    scode = apr_strtok(NULL, &delimiter, &state);

    if (scode[0] == '2' || scode[0] == '3') {
        *verified = FLOOD_VALID;
    }
    else {
        *verified = FLOOD_INVALID;
    }

    return APR_SUCCESS;
}

int round_robin_loop_condition(profile_t *profile)
{
    round_robin_profile_t *rp;
    int real_current_url;

    rp = (round_robin_profile_t*)profile;

    real_current_url = rp->current_url; /* save the real one before we try to increment */

    rp->current_url++;

    /* Adjust counters for profile */
    if (rp->current_url >= rp->urls) {
        rp->current_url = 0;
        
        /* Loop cond tells us when to stop. */
        rp->current_round++;
    }

#ifdef PROFILE_DEBUG
    apr_file_printf(local_stdout, "Round %d of %d, %s.\n",
                    rp->current_round, rp->execute_rounds,
                    (rp->current_round < rp->execute_rounds ? "Continuing" : "Finished"));
#endif /* PROFILE_DEBUG */

    if (rp->current_round >= rp->execute_rounds)
        return 0;
    else { /* we'll continue, so do delay stuff now if necessary */
        
        /* If they want a sleep, do it now. */
        if (rp->url[real_current_url].postdelay) {
            apr_int64_t real_postdelay = rp->url[real_current_url].postdelay;

            /* If the delay has a precision, adjust the
             * delay by some random fraction of the precision here */
            if (rp->url[real_current_url].postdelayprecision) {
                /* FIXME: this should be more portable, like apr_generate_random_bytes() */
                float factor = -1.0 + (2.0*rand()/(RAND_MAX+1.0));
                real_postdelay += rp->url[real_current_url].postdelayprecision * factor;
            }

            /* we can only delay positive times, can't go back in time :( */
            if (real_postdelay < 0)
                real_postdelay = 0;

            /* only bother going to sleep if we generated a delay */
            if (real_postdelay > 0)
                apr_sleep(real_postdelay);
        }

        return 1;
    }
}

apr_status_t round_robin_profile_destroy(profile_t *profile)
{
    /* FIXME: free() the memory used by this profile, or reset() the pool
     * (or whatever semantics apr uses, I dunno...) -aaron */
    return APR_SUCCESS;
}

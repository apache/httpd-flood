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

#ifndef __profile_h
#define __profile_h

#include <apr_network_io.h> /* Required for apr_socket_t */
#include <apr_pools.h>
#include <apr_uri.h>

#include "flood_config.h" /* Required for config_t */

/* Symbolic definitions for response verification results.
 * 0 is valid, non-zero is a type of invalid.
 */
#define FLOOD_VALID 0
#define FLOOD_INVALID 1

/* The type of buffers that are allowable internally in the flood
 * architecture.  Normally, test clients will not be concerned with
 * this. */
enum buffer_type_e {
    HEAP,
    POOL,
    FD,
    MMAP
};
typedef enum buffer_type_e buffer_type_e;

/* Allowable HTTP methods that the flood architecture currently has
 * knowledge of. */
enum method_e {
    GET,
    POST,
    HEAD
};
typedef enum method_e method_e;

/**
 * Holds the socket information.
 */ 
typedef void socket_t;

/* Define a single request that can be transmitted with the flood
 * architecture. */
struct request_t {
    /* The basic components to connect to the server */
    char * uri;
    char * proxy_url;

    method_e method;
    int keepalive; /* a boolean */

    /* Following only valid when method == POST */
    apr_size_t payloadsize;
    void * payload;
    
    apr_uri_components *parsed_uri;

    /* Raw buffer connection 
     * FIXME: apr_bucket_t? */ 
    buffer_type_e rbuftype;
    void * rbuf;
    apr_size_t rbufsize;

    /* Mandatory for keepalives - although we aren't handling keepalives
     * just yet... */
    socket_t *rsock;

    apr_pool_t *pool;
};
typedef struct request_t request_t;

/* Define a single response that may be returned with the flood
 * architecture. */
struct response_t {
    /* Raw buffer connection 
     * FIXME: apr_bucket_t? */ 
    buffer_type_e rbuftype;
    void * rbuf;
    apr_size_t rbufsize;
};
typedef struct response_t response_t;

/**
 * profile_t holds all stateful data needed during the running of
 * a test. For example, if we wanted to test a single URL 10 times
 * and then quit, profile_t would hold that URL plus a counter.
 * The actual data stored in profile_t is specific to the implementation
 * of the test.
 */
typedef void profile_t;

/**
 * Opaque data to hold any type of reporting data.
 */
typedef void report_t;

struct profile_events_t {
    /**
     * Reads a config_t object and creates any state necessary for this
     * run of a test profile. This routine is typically run only once
     * per single instance of a test profile.
     * Returns: State object for this instance of a test profile.
     */
    apr_status_t (*profile_init)(profile_t **profile, config_t *config, const char *profile_name, apr_pool_t *pool);

    /**
     * Create/initialize a report structure that will be associated with
     * this profile and will exist for the same lifetime as this profile.
     * However, the data stored in this report is not coupled with the
     * profile data, and the implementation may vary.
     * Returns: APR_SUCCESS and sets report to the new instance, or
     * an appropriate error otherwise.
     */
    apr_status_t (*report_init)(report_t **report, config_t *config, const char *profile_name, apr_pool_t *pool);

    /**
     * Reads the profile state (profile_t), retrieves the next request
     * (really the next URL) in this test profile, and prepares a
     * request.
     * Returns: A prepared HTTP Request object, ready to send.
     */
    apr_status_t (*get_next_url)(request_t **request, profile_t *profile);

    /**
     * Actually sends the request to the server. Implementation will fit
     * an HTTP function or some OS performance capability.
     * Returns: The bi-directional socket used for this HTTP transaction.
     */
    apr_status_t (*send_req)(socket_t **sock, request_t *req, apr_pool_t *pool);

    /**
     * Receives the request from the server. Implementation will test
     * some function of HTTP or some OS performance capability.
     * Returns: The response returned from this socket.
     */
    apr_status_t (*recv_resp)(response_t **resp, socket_t *sock, apr_pool_t *pool);

    /**
     * Does any post processing on this request cycle. This is where
     * SetCookie handling goes, or any other client-side-state.
     * (also stuff like "Follow Location: redirects).
     * Returns APR_SUCCESS or else some APR_* error.
     */
    apr_status_t (*postprocess)(profile_t *profile,
                                request_t *req,
                                response_t *resp);

    /**
     * Tests this HTTP transaction (both Request and Response) to see if it
     * is valid for this particular test profile. Also may need access to the
     * state data for this test profile.
     * Returns: boolean determining if this request/response was valid.
     */
    apr_status_t (*verify_resp)(int *verified, profile_t *profile, request_t *req, response_t *resp);

    /**
     * Callback to this test profile so it can report on this particular
     * HTTP transaction. It is guaranteed that this function is called only once
     * per HTTP transaction, and immediatly after "verify_resp" is called.
     * Returns: boolean status of this function.
     */
    apr_status_t (*process_stats)(report_t *report, int verified);

    /**
     * Test to see if this test profile will continue with further tests or 
     * complete.
     * Returns: boolean
     */
    int (*loop_condition)(profile_t *profile);

    /**
     * Destroy the given request, which should be the request used
     * during this pass of the test. This is called at the end
     * of every successful pass through the test loop.
     * Returns: apr_status_t signifying completion status
     */
    apr_status_t (*request_destroy)(request_t *req);

    /**
     * Destroy the given response, which should be the response used
     * during this pass ofo the test. This is called at the end
     * of every successful pass through the test loop.
     * Returns: apr_status_t signifying completion status
     */
    apr_status_t (*response_destroy)(response_t *resp);

    /**
     * Destroy the socket used during with this pass, for some
     * request/response pair. This is called at the end of
     * every successful pass through the test loop.
     * Returns: apr_status_t signifying completion status
     */
    apr_status_t  (*socket_destroy)(socket_t *socket);

    /**
     * Generate a report on all statistical information
     * gathered during this run of the profile.
     */
    apr_status_t (*report_stats)(report_t *report);

    /**
     * Destroy the report structure associated with this proile.
     * Returns: APR_SUCCESS or an appropriate error.
     */
    apr_status_t (*destroy_report)(report_t *report);

    /**
     * Destroy this profile. Cleanup routines that complement the above
     * profile_init function. This function is called only after the
     * test has successfully completed.
     * Returns: apr_status_t signifying completion status
     */
    apr_status_t  (*profile_destroy)(profile_t *profile);

};
typedef struct profile_events_t profile_events_t;

apr_status_t run_profile(apr_pool_t *pool, config_t *config, const char *profile_name);

#endif  /* __profile_h */

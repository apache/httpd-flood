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

#ifndef __profile_h
#define __profile_h

#include <apr_network_io.h> /* Required for apr_socket_t */
#include <apr_tables.h>     /* Required for apr_table_t */
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
    
    apr_uri_t *parsed_uri;

    /* Raw buffer connection 
     * FIXME: apr_bucket_t? */ 
    buffer_type_e rbuftype;
    void * rbuf;
    apr_size_t rbufsize;

    /* If this is set, we want to keep the *entire* response. */
    int wantresponse;

    /* Mandatory for keepalives - although we aren't handling keepalives
     * just yet... */
    socket_t *rsock;

    apr_pool_t *pool;
};
typedef struct request_t request_t;

/* Define a single response that may be returned with the flood
 * architecture. */
struct response_t {
    /* a boolean */
    int keepalive; 
    /* a boolean */
    int chunked;
    char *chunk;

    /* Raw buffer connection 
     * FIXME: apr_bucket_t? */ 
    buffer_type_e rbuftype;
    char *rbuf;
    apr_size_t rbufsize;

    apr_table_t *headers;
};
typedef struct response_t response_t;

/* Define a timer. */
struct flood_timer_t {
    apr_time_t begin;
    apr_time_t connect;
    apr_time_t write;
    apr_time_t read;
    apr_time_t close;
};
typedef struct flood_timer_t flood_timer_t;

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

    /* Profile Events */
    /**
     * Reads a config_t object and creates any state necessary for this
     * run of a test profile. This routine is typically run only once
     * per single instance of a test profile.
     * Returns: State object for this instance of a test profile.
     */
    apr_status_t (*profile_init)(profile_t **profile, config_t *config, const char *profile_name, apr_pool_t *pool);

    /**
     * Reads the profile state (profile_t), retrieves the next request
     * (really the next URL) in this test profile, and prepares a
     * request.
     * Returns: A prepared HTTP Request object, ready to send.
     */
    apr_status_t (*get_next_url)(request_t **request, profile_t *profile);

    /**
     * Construct the request to be sent to the server.
     */
    apr_status_t (*create_req)(profile_t *p, request_t *r);

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
     * Test to see if this test profile will continue with further tests or 
     * complete.
     * Returns: boolean
     */
    int (*loop_condition)(profile_t *profile);

    /**
     * Destroy this profile. Cleanup routines that complement the above
     * profile_init function. This function is called only after the
     * test has successfully completed.
     * Returns: apr_status_t signifying completion status
     */
    apr_status_t  (*profile_destroy)(profile_t *profile);




    /* Reporting Events */
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
     * Callback to this test profile so it can report on this particular
     * HTTP transaction. It is guaranteed that this function is called only once
     * per HTTP transaction, and immediatly after "verify_resp" is called.
     * Returns: boolean status of this function.
     */
    apr_status_t (*process_stats)(report_t *report, int verified, request_t *req, response_t *resp, flood_timer_t *timer);

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





    /* Socket Events */
    /**
     * Create and initialize the communication channel that
     * we'll use for this test.
     */
    apr_status_t (*socket_init)(socket_t **sock, apr_pool_t *pool);

    /**
     * Opens the communication channel to the server.
     */
    apr_status_t (*begin_conn)(socket_t *sock, request_t *req, apr_pool_t *pool);

    /**
     * Actually sends the request to the server. Implementation will fit
     * an HTTP function or some OS performance capability.
     */
    apr_status_t (*send_req)(socket_t *sock, request_t *req, apr_pool_t *pool);

    /**
     * Receives the request from the server. Implementation will test
     * some function of HTTP or some OS performance capability.
     */
    apr_status_t (*recv_resp)(response_t **resp, socket_t *sock, apr_pool_t *pool);

    /**
     * Tears down the communication channel to the server. Called once per pass
     * through main request/response loop. Implementation typically closes
     * the socket.
     */
    apr_status_t (*end_conn)(socket_t *sock, request_t *req, response_t *resp);

    /**
     * Destroy the given request, which should be the request used
     * during this pass of the test. This is called at the end
     * of every successful pass through the test loop.
     */
    apr_status_t (*request_destroy)(request_t *req);

    /**
     * Destroy the given response, which should be the response used
     * during this pass of the test. This is called at the end
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





    /* Verify Events */
    /**
     * Tests this HTTP transaction (both Request and Response) to see if it
     * is valid for this particular test profile. Also may need access to the
     * state data for this test profile.
     * Returns: boolean determining if this request/response was valid.
     */
    apr_status_t (*verify_resp)(int *verified, profile_t *profile, request_t *req, response_t *resp);

};
typedef struct profile_events_t profile_events_t;

apr_status_t run_profile(apr_pool_t *pool, config_t *config, const char *profile_name);

#endif  /* __profile_h */

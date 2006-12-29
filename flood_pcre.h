/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Derived from PCRE's pcreposix.h.

            Copyright (c) 1997-2004 University of Cambridge

-----------------------------------------------------------------------------
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

    * Neither the name of the University of Cambridge nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
-----------------------------------------------------------------------------
*/

/**
 * @file flood_regex.h
 * @brief Apache Regex defines
 */

#ifndef FLOOD_REGEX_H
#define FLOOD_REGEX_H

#include "apr.h"

/* Allow for C++ users */

#ifdef __cplusplus
extern "C" {
#endif

/* Options for flood_regexec: */

#define FLOOD_REG_ICASE    0x01 /** use a case-insensitive match */
#define FLOOD_REG_NEWLINE  0x02 /** don't match newlines against '.' etc */
#define FLOOD_REG_NOTBOL   0x04 /** ^ will not match against start-of-string */
#define FLOOD_REG_NOTEOL   0x08 /** $ will not match against end-of-string */

#define FLOOD_REG_EXTENDED (0)  /** unused */
#define FLOOD_REG_NOSUB    (0)  /** unused */

/* Error values: */
enum {
  FLOOD_REG_ASSERT = 1,  /** internal error ? */
  FLOOD_REG_ESPACE,      /** failed to get memory */
  FLOOD_REG_INVARG,      /** invalid argument */
  FLOOD_REG_NOMATCH      /** match failed */
};

/* The structure representing a compiled regular expression. */
typedef struct {
    void *re_pcre;
    apr_size_t re_nsub;
    apr_size_t re_erroffset;
} flood_regex_t;

/* The structure in which a captured offset is returned. */
typedef struct {
    int rm_so;
    int rm_eo;
} flood_regmatch_t;

/* The functions */

/**
 * Compile a regular expression.
 * @param preg Returned compiled regex
 * @param regex The regular expression string
 * @param cflags Must be zero (currently).
 * @return Zero on success or non-zero on error
 */
int flood_regcomp(flood_regex_t *preg, const char *regex,
                  int cflags);

/**
 * Match a NUL-terminated string against a pre-compiled regex.
 * @param preg The pre-compiled regex
 * @param string The string to match
 * @param nmatch Provide information regarding the location of any matches
 * @param pmatch Provide information regarding the location of any matches
 * @param eflags Bitwise OR of any of FLOOD_REG_* flags 
 * @return 0 for successful match, #REG_NOMATCH otherwise
 */ 
int flood_regexec(const flood_regex_t *preg, const char *string,
                  apr_size_t nmatch, flood_regmatch_t *pmatch, int eflags);

/**
 * Return the error code returned by regcomp or regexec into error messages
 * @param errcode the error code returned by regexec or regcomp
 * @param preg The precompiled regex
 * @param errbuf A buffer to store the error in
 * @param errbuf_size The size of the buffer
 */
apr_size_t flood_regerror(int errcode, const flood_regex_t *preg, 
                          char *errbuf, apr_size_t errbuf_size);

/** Destroy a pre-compiled regex.
 * @param preg The pre-compiled regex to free.
 */
void flood_regfree(flood_regex_t *preg);

/* Expose our API as the POSIX compatibility layer */
#define regcomp flood_regcomp
#define regexec flood_regexec
#define regfree flood_regfree
#define regex_t flood_regex_t
#define regmatch_t flood_regmatch_t
#define REG_EXTENDED FLOOD_REG_EXTENDED

#ifdef __cplusplus
}   /* extern "C" */
#endif

#endif /* FLOOD_REGEX_T */


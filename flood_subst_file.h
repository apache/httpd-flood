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
 *
 */
#ifndef _SUBST_FILE_H
#define _SUBST_FILE_H 1

#define SUBST_FILE_MAX_URL_SIZE 8096
#define SUBST_FILE_ERROR_BUF 256
#define SUBST_FILE_ARR_MAX 10
struct subst_rec_t {
  char* subst_var;
  char* subst_file_name;
  apr_file_t* subst_file;
  int subst_mode;
  apr_off_t fsize;
  int valid;
};
typedef struct subst_rec_t subst_rec_t;

void subst_file_err(const char*, const char*, apr_status_t);
int subst_file_open(apr_file_t**, const char*, apr_off_t*, apr_pool_t*);
int close_subst_file(apr_file_t*);
char* subst_file_entry_get(apr_file_t**, apr_off_t*, char*, int);
subst_rec_t* subst_file_get(const char*, subst_rec_t*);

#endif

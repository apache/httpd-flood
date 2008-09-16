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
#include <apr_general.h>
#include <apr_file_io.h>

#include "flood_subst_file.h"

#include <assert.h>
#include <stdlib.h>

#define IS_NUM(c)       (('0' <= (c)) && ('9' >= (c)))

extern apr_file_t *local_stdout;
extern apr_file_t *local_stderr;

void subst_list_init(subst_rec_t *subst_list, int subst_list_size) {
  int i;

  for (i = 0; i < SUBST_FILE_ARR_MAX; i++) {
    subst_list[i].subst_var = NULL;
    subst_list[i].subst_file_name = NULL;
    subst_list[i].subst_mode = 0;
    subst_list[i].fsize = (apr_off_t)0;
    subst_list[i].valid = 0;
    subst_list[i].subst_file = NULL;
  }
}

subst_rec_t* subst_file_get(const char* varname, subst_rec_t* subst_list) {
  int i;

  for (i = 0; i < SUBST_FILE_ARR_MAX; i++) {
    if ((strcmp(subst_list[i].subst_var, varname) == 0)
        && subst_list[i].valid) {
      return &(subst_list[i]);
    }
  }
  return NULL;
}

void subst_file_err(const char* msgtext, const char* vartext, apr_status_t errcode) {
  char errtext[SUBST_FILE_ERROR_BUF];
  apr_pool_t *err_pool;

  if (apr_pool_create(&err_pool, NULL) != APR_SUCCESS) {
    printf("Failed apr_pool_create\n");
    exit(-1);
  }

  apr_strerror(errcode, (char *) &errtext, SUBST_FILE_ERROR_BUF);

  apr_file_printf(local_stderr, "%s %s %s\n", msgtext, vartext, errtext);

  apr_pool_destroy(err_pool);
}

int subst_file_open(apr_file_t** subst_file, const char* fname,
                    apr_off_t* fsize, apr_pool_t* pool) {
  apr_finfo_t finfo;
  apr_status_t rc = 0;
  apr_int32_t wanted = APR_FINFO_SIZE;

  rc = apr_file_open(subst_file, fname, APR_READ, APR_OS_DEFAULT, pool);

  if (rc) {
    subst_file_err("Couldn't open file", fname, rc);
    exit(-1);
  }

  rc = 0;
  if (rc = apr_stat(&finfo, fname, wanted, pool)) {
    subst_file_err("stat failed on file ", fname, rc);
    apr_file_close(*subst_file);
    exit(-1);
  }
  *fsize = finfo.size;

  return 0;
}

char* subst_file_entry_get(apr_file_t** subst_file, apr_off_t *fsize, char* line, int line_size) {
  apr_off_t seek_val;
  apr_off_t zero = 0;
  apr_status_t rc = 0;

  if (!subst_file ) {
    subst_file_err("subst_file not open ", "", rc);
    exit(-1);
  }
  assert (line_size > 0);
  assert (*fsize > 0);
  seek_val = random() % *fsize;

  if (apr_file_seek(*subst_file, APR_SET, &seek_val) != 0 )  {
    subst_file_err("error in seeking for file", "no name available", rc);    
    exit(-1 );
  }

  apr_file_gets(line, line_size, *subst_file);
  memset(line, 0, line_size);
  if (apr_file_gets(line, line_size, *subst_file) != (apr_status_t)0 ) {
    if (apr_file_seek(*subst_file, APR_SET, &zero) != (apr_off_t)0 )  {
      subst_file_err("error in seeking for file", "no name available", rc);    
      exit(-1 );
    }
    apr_file_gets(line, line_size, *subst_file);
  }
  line[strlen(line)-1] = '\0';
  return line;
}

/* a substitution file entry (nomimally a single line) can now contain */
/* escaped characters such as \n, \t, \012 so that the entry can be */
/* a multi-line POST payload */
char* subst_file_entry_unescape(char* line, int line_size)
{
  int num;
  char *from, *to;
  char changed_buf[SUBST_FILE_MAX_URL_SIZE];
 
 if (line == NULL) {
    return NULL;
  }

  from = to = line;
  while (*from) {
    if (*from == '\\') {
      ++from;
      if (IS_NUM(*from)) {
          num = *from++ - '0';
          if (IS_NUM(*from))
              num = num*10 + (*from++ - '0');
          if (IS_NUM(*from))
              num = num*10 + (*from++ - '0');
          if (num != 0) {
              *to++ = num;
          } else {
              *to++ = '\\';
              *to++ = '0';
          }
      } else {
        switch (*from) {
        case '\0': continue;
        case 'n': *to++ = '\n'; break;
        case 'r': *to++ = '\r'; break;
        case 't': *to++ = '\t'; break;
        default: *to++ = *from; break;
        }
        ++from;
      }
    } else {
      *to++ = *from++;
    }
  }
  *to = '\0';

  return line;
}

#ifdef SUBST_MAIN
subst_rec_t subst_list[SUBST_FILE_ARR_MAX];

void subst_list_make(subst_rec_t *subst_list) {
  int i2 = 0;

  subst_list[0].subst_var = "name";
  subst_list[0].subst_file_name = "/pmalab1/temphome/guyf/work/replace_mc5/flood_stuff/build/flood-0.4/test";
  subst_list[0].subst_mode = 0;
  subst_list[0].valid = 1;
  i2++;

  subst_list[i2].subst_var = "foot";
  subst_list[i2].subst_file_name = "/pmalab1/temphome/guyf/work/replace_mc5/flood_stuff/build/flood-0.4/blort";
  subst_list[i2].subst_mode = 0;
  subst_list[i2].valid = 1;

  i2++;
  subst_list[i2].subst_var = "nerve";
  subst_list[i2].subst_file_name = "/pmalab1/temphome/guyf/work/replace_mc5/flood_stuff/build/flood-0.4/cavort";
  subst_list[i2].subst_mode = 0;
  subst_list[i2].valid = 1;
}

int close_subst_file(apr_file_t* subst_file) {
  apr_status_t rc = 0;

  if (subst_file) {
    rc = apr_file_close(subst_file);
  }
  return rc;
}

int main(int argc, char** argv) {
  char line[SUBST_FILE_MAX_URL_SIZE];
  int i = 20;
  int list = 0;
  char* the_name;
  subst_rec_t *the_rec;

  apr_initialize();
  atexit(apr_terminate);

  apr_file_open_stderr(&local_stderr, err_pool);

  if (apr_pool_create(&pool, NULL) != APR_SUCCESS) {
    printf("Failed apr_pool_create\n");
    exit(-1);
  }

  srandom(time(NULL));

  subst_list_init(subst_list, SUBST_FILE_ARR_MAX);
  subst_list_make(subst_list);

  while(subst_list[list].valid && list < 10) {
    subst_file_open(&(subst_list[list].subst_file), subst_list[list].subst_file_name, &(subst_list[list].fsize));
    list++;
  }

/*   list = 0; */
/*   while (i--) { */
/*     while(subst_list[list].valid && list < 10) { */
/*       subst_file_entry_get(&(subst_list[list].subst_file), &(subst_list[list].fsize), line, sizeof(line)); */
/*       printf("%s\n", line); */
/*       memset(line, 0, sizeof(line));       */
/*       list++; */
/*     } */
/*     list = 0; */
/*   } */

  the_name = "test";
  the_rec = subst_file_get(the_name, subst_list);
  subst_file_entry_get(&(the_rec->subst_file), &(the_rec->fsize), line, sizeof(line));
  printf("%s\n", line);

  list = 0;
  while(subst_list[list].valid && list < 10) {
    close_subst_file(subst_list[list].subst_file);
    list++;
  }

  exit(0);
}

#endif

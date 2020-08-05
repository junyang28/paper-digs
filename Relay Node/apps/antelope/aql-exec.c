/*
 * Copyright (c) 2010, Swedish Institute of Computer Science
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */

/**
 * \file
 *	Query execution functions for AQL.
 * \author
 * 	Nicolas Tsiftes <nvt@sics.se>
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define DEBUG DEBUG_NONE
#include "net/ip/uip-debug.h"

#include "index.h"
#include "relation.h"
#include "result.h"
#include "aql.h"

static aql_adt_t adt;

static void
clear_handle(db_handle_t *handle)
{
  memset(handle, 0, sizeof(*handle));

  handle->result_rel = NULL;
  handle->left_rel = NULL;
  handle->right_rel = NULL;
  handle->join_rel = NULL;
}

static db_result_t
aql_execute(db_handle_t *handle, aql_adt_t *adt)
{
  uint8_t optype;
  int first_rel_arg;
  db_result_t result;
  relation_t *rel;
  aql_attribute_t *attr;
  attribute_t *relattr;

  optype = AQL_GET_TYPE(adt);
  if(optype == AQL_TYPE_NONE) {
    /* No-ops always succeed. These can be generated by
       empty lines or comments in the query language. */
    return DB_OK;
  }

  /* If the ASSIGN flag is set, the first relation in the array is
     the desired result relation. */
  first_rel_arg = !!(adt->flags & AQL_FLAG_ASSIGN);

  if(optype != AQL_TYPE_CREATE_RELATION &&
     optype != AQL_TYPE_REMOVE_RELATION &&
     optype != AQL_TYPE_JOIN) {
    rel = relation_load(adt->relations[first_rel_arg]);
    if(rel == NULL) {
      return DB_NAME_ERROR;
    }
  } else {
    rel = NULL;
  }

  result = DB_RELATIONAL_ERROR;
  switch(optype) {
  case AQL_TYPE_CREATE_ATTRIBUTE:
    attr = &adt->attributes[0];
    if(relation_attribute_add(rel, DB_STORAGE, attr->name, attr->domain, 
       attr->element_size) != NULL) {
      result = DB_OK;
    }
    break;
  case AQL_TYPE_CREATE_INDEX:
    relattr = relation_attribute_get(rel, adt->attributes[0].name);
    if(relattr == NULL) {
      result = DB_NAME_ERROR;
      break;
    }
    result = index_create(AQL_GET_INDEX_TYPE(adt), rel, relattr);
    break;
  case AQL_TYPE_CREATE_RELATION:
    if(relation_create(adt->relations[0], DB_STORAGE) != NULL) {
      result = DB_OK;
    }
    break;
  case AQL_TYPE_REMOVE_ATTRIBUTE:
    result = relation_attribute_remove(rel, adt->attributes[0].name);
    break;
  case AQL_TYPE_REMOVE_INDEX:
    relattr = relation_attribute_get(rel, adt->attributes[0].name);
    if(relattr != NULL) {
      if(relattr->index != NULL) {
        result = index_destroy(relattr->index);
      } else {
        result = DB_OK;
      }
    } else {
      result = DB_NAME_ERROR;
    }
    break;
  case AQL_TYPE_REMOVE_RELATION:
    result = relation_remove(adt->relations[0], 1);
    break;
#if DB_FEATURE_REMOVE
  case AQL_TYPE_REMOVE_TUPLES:
     /* Overwrite the attribute array with a full copy of the original 
        relation's attributes. */
    adt->attribute_count = 0;
    for(relattr = list_head(rel->attributes);
        relattr != NULL;
        relattr = relattr->next) {
      AQL_ADD_ATTRIBUTE(adt, relattr->name, DOMAIN_UNSPECIFIED, 0);
    }
    AQL_SET_FLAG(adt, AQL_FLAG_INVERSE_LOGIC);
#endif /* DB_FEATURE_REMOVE */
  case AQL_TYPE_SELECT:
    if(handle == NULL) {
      result = DB_ARGUMENT_ERROR;
      break;
    }
    result = relation_select(handle, rel, adt);
    break;
  case AQL_TYPE_INSERT:
    result = relation_insert(rel, adt->values);
    break;
#if DB_FEATURE_JOIN
  case AQL_TYPE_JOIN:
    if(handle == NULL) {
      result = DB_ARGUMENT_ERROR;
      break;
    }
    handle->left_rel = relation_load(adt->relations[first_rel_arg]);
    if(handle->left_rel == NULL) {
      break;
    }
    handle->right_rel = relation_load(adt->relations[first_rel_arg + 1]);
    if(handle->right_rel == NULL) {
      relation_release(handle->left_rel);
      break;
    }
    result = relation_join(handle, adt);
    break;
#endif /* DB_FEATURE_JOIN */
  default:
    break;
  }

  if(rel != NULL) {
    if(handle == NULL || !(handle->flags & DB_HANDLE_FLAG_PROCESSING)) {
      relation_release(rel);
    }
  }

  return result;
}

db_result_t
db_query(db_handle_t *handle, const char *format, ...)
{
  va_list ap;
  char query_string[AQL_MAX_QUERY_LENGTH];

  va_start(ap, format);
  vsnprintf(query_string, sizeof(query_string), format, ap);
  va_end(ap);

  if(handle != NULL) {
    clear_handle(handle);
  }

  if(AQL_ERROR(aql_parse(&adt, query_string))) {
    return DB_PARSING_ERROR;
  }

  /*aql_optimize(&adt);*/

  return aql_execute(handle, &adt);
}

db_result_t
db_process(db_handle_t *handle)
{
  uint8_t optype;

  optype = ((aql_adt_t *)handle->adt)->optype;

  switch(optype) {
#if DB_FEATURE_REMOVE
  case AQL_TYPE_REMOVE_TUPLES:
    return relation_process_remove(handle);
    break;
#endif
  case AQL_TYPE_SELECT:
    return relation_process_select(handle);
    break;
#if DB_FEATURE_JOIN
  case AQL_TYPE_JOIN:
    return relation_process_join(handle);
#endif /* DB_FEATURE_JOIN */
  default:
    break;
  }

  PRINTF("DB: Invalid operation type: %d\n", optype);

  return DB_INCONSISTENCY_ERROR;
}

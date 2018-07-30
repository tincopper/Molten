/**
 * Copyright 2017 chuan-yun silkcutKs <silkcutbeta@gmail.com>
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
 */

#ifndef MOLTEN_SPAN_H
#define MOLTEN_SPAN_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "stdint.h"
#include <netinet/in.h>

#include "php.h"

#include "php7_wrapper.h"
#include "molten_struct.h"
#include "molten_util.h"
#include "molten_stack.h"

/* log format */
#define ZIPKIN      1
#define OPENTRACING 1<<1
#define SKYWALKING  1<<2

/* annotation type */
#define AN_SERVER      0
#define AN_CLIENT      1

#define SK_REGISTER_APPLICATION  "application/register"
#define SK_REGISTER_SERVICE      "instance/register"
#define SK_SERVICENAME_DISCOVERY "servicename/discovery"
#define SK_INSTANCE_HEARTBEAT    "instance/heartbeat"
#define SK_SEGMENTS              "segments"

/* ba type */
enum ba_type {BA_NORMAL, BA_SA, BA_SA_HOST, BA_SA_IP, BA_SA_DSN, BA_ERROR, BA_PATH};

/* span id builder */
typedef void (*build_span_id_func)(char **span_id, char *parent_span_id, int span_count);

/* span_builder */
typedef void (*start_span_func)(zval **span, char *service_name, char *trace_id, char *span_id, char *parent_id, long start_time, long finish_time, struct mo_chain_st *pct, uint8_t an_type);
typedef void (*start_span_ex_func)(zval **span, char *service_name, struct mo_chain_st *pct, mo_frame_t *frame, uint8_t an_type);
typedef void (*span_add_ba_func)(zval *span, const char *key, const char *value, long timestamp, char *service_name, char *ipv4, long port, uint8_t ba_type);
typedef void (*span_add_ba_ex_func)(zval *span, const char *key, const char *value, long timestamp, struct mo_chain_st *pct, uint8_t ba_type);

typedef struct {
    uint8_t                 type;
    uint32_t                application_id; //add by tangzy at 20180730
    uint32_t                instance_id;   //add by tangzy at 20180730
    start_span_func         start_span;
    start_span_ex_func      start_span_ex;
    span_add_ba_func        span_add_ba;
    span_add_ba_ex_func     span_add_ba_ex;
}mo_span_builder;

/* {{{ span builder for two format */
//zipkin
void zn_start_span_builder(zval **span, char *service_name, char *trace_id, char *span_id, char *parent_id, long start_time, long finish_time, struct mo_chain_st *pct, uint8_t an_type);
void zn_start_span_ex_builder(zval **span, char *service_name, struct mo_chain_st *pct, mo_frame_t *frame, uint8_t an_type);
void zn_span_add_ba_builder(zval *span, const char *key, const char *value, long timestamp, char *service_name, char *ipv4, long port, uint8_t ba_type);
void zn_span_add_ba_ex_builder(zval *span, const char *key, const char *value, long timestamp, struct mo_chain_st *pct, uint8_t ba_type);

//opentracing
void ot_start_span_builder(zval **span, char *service_name, char *trace_id, char *span_id, char *parent_id, long start_time, long finish_time, struct mo_chain_st *pct, uint8_t an_type);
void ot_start_span_ex_builder(zval **span, char *service_name, struct mo_chain_st *pct, mo_frame_t *frame, uint8_t an_type);
void ot_span_add_ba_builder(zval *span, const char *key, const char *value, long timestamp, char *service_name, char *ipv4, long port, uint8_t ba_type);
void ot_span_add_ba_ex_builder(zval *span, const char *key, const char *value, long timestamp, struct mo_chain_st *pct, uint8_t ba_type);

//skywalking
void sk_register_application_builder(zval **span);
void sk_register_service_builder(char *res_data, int application_id, char *agent_uuid);
void sk_segments_builder();
void sk_start_span_builder(zval **span, char *service_name, char *trace_id, char *span_id, char *parent_id, long start_time, long finish_time, struct mo_chain_st *pct, uint8_t an_type);
void sk_start_span_ex_builder(zval **span, char *service_name, struct mo_chain_st *pct, mo_frame_t *frame, uint8_t an_type);
void sk_span_add_ba_builder(zval *span, const char *key, const char *value, long timestamp, char *service_name, char *ipv4, long port, uint8_t ba_type);
void sk_span_add_ba_ex_builder(zval *span, const char *key, const char *value, long timestamp, struct mo_chain_st *pct, uint8_t ba_type);
/* }}} */

/* {{{ span context for build trace context relation */

/* the traceId and sampled can use global, no need copy point for every context */
#ifdef USE_LEVEL_ID
typedef struct {
    char *span_id;
    int span_count;
} mo_span_context;
#endif

void init_span_context(mo_stack *stack);
void push_span_context(mo_stack *stack);
void push_span_context(mo_stack *stack);
void push_span_context_with_id(mo_stack *stack, char *span_id);
void pop_span_context(mo_stack *stack);
void retrieve_span_id(mo_stack *stack, char **span_id);
void retrieve_parent_span_id(mo_stack *stack, char **parent_span_id);
void destroy_span_context(mo_stack *stack);

void retrieve_span_id_4_frame(mo_frame_t *frame, char **span_id);
void retrieve_parent_span_id_4_frame(mo_frame_t *frame, char **parent_span_id);
/* }}} */


/* {{{ pt span ctor */
static void inline mo_span_ctor(mo_span_builder *psb, char *span_format)
{
    if (strncmp(span_format, "zipkin", sizeof("zipkin") - 1) == 0) {
        psb->type = ZIPKIN;
        psb->start_span         = &zn_start_span_builder;
        psb->start_span_ex      = &zn_start_span_ex_builder;
        psb->span_add_ba        = &zn_span_add_ba_builder;
        psb->span_add_ba_ex     = &zn_span_add_ba_ex_builder;
    } else if (strncmp(span_format, "opentracing", sizeof("opentracing") - 1) == 0) {
        psb->type = OPENTRACING;
        psb->start_span         = &ot_start_span_builder;
        psb->start_span_ex      = &ot_start_span_ex_builder;
        psb->span_add_ba        = &ot_span_add_ba_builder;
        psb->span_add_ba_ex     = &ot_span_add_ba_ex_builder;
    } else {
        psb->type = SKYWALKING;
        psb->start_span         = &sk_start_span_builder;
        psb->start_span_ex      = &sk_start_span_ex_builder;
        psb->span_add_ba        = &sk_span_add_ba_builder;
        psb->span_add_ba_ex     = &sk_span_add_ba_ex_builder;
    }
}
/* }}} */

void mo_span_pre_init_ctor(mo_span_builder *psb, char* sink_http_uri, char* service_name);

/* {{{ record fucntion for zipkin format, it is the default set */
void zn_sart_span(zval **span, char *trace_id, char *service_name, char *span_id, char *parent_id, long timestamp, long duration);
void zn_add_span_annotation(zval *span, const char *value, long timestamp, char *service_name, char *ipv4, long port);
void zn_add_span_annotation_ex(zval *span, const char *value, long timestamp, struct mo_chain_st *pct);
void zn_add_span_bannotation(zval *span, const char *key, const char *value, char *service_name, char *ipv4, long port);
void zn_add_span_bannotation_ex(zval *span, const char *key, const char *value, struct mo_chain_st *pct);
/* }}} */

/* {{{ record fucntion for skywalking format, it is the default set */
char *sk_get_server(char *url);
int sk_register_application(char *name, char *server_url);
int sk_register_instance(int application_id, char *server_url);
void sk_log_segments(int application_id, int instance_id);
/* }}} */

#endif

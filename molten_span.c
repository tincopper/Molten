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

#include <string.h>
#include <pthread.h>
#include "common/host_info.h"
#include "molten_span.h"
#include "common/molten_http_util.h"
#include "common/molten_cJSON.h"
#include "common/load_balance.h"
#include "php_molten.h"
#include "curl/curl.h"


/* use span context */
/* {{{ build span context */
void span_context_dtor(void *element)
{
#ifdef USE_LEVEL_ID
    mo_span_context *context = (mo_span_context *)element;
    efree(context->span_id);
    context->span_count = 0;
#else
    efree(*(char **)element);
#endif
}
/* }}} */

/* {{{ init span context */
void init_span_context(mo_stack *stack)
{
#ifdef USE_LEVEL_ID
    mo_stack_init(stack, sizeof(mo_span_context), &span_context_dtor);
#else
    mo_stack_init(stack, sizeof(char *), &span_context_dtor);
#endif
}
/* }}} */

/* {{{ push span context */
void push_span_context(mo_stack *stack)
{
#ifdef USE_LEVEL_ID
    char *span_id;
    mo_span_context context;
    if (!mo_stack_empty(stack)) {
        mo_span_context *parent_node = (mo_span_context *)mo_stack_top(stack);
        build_span_id_level(&span_id, parent_node->span_id, parent_node->span_count);
        parent_node->span_count++;
    } else {
        build_span_id_level(&span_id, NULL, 0);
    }

    context.span_id = span_id;
    context.span_count = 1;
    mo_stack_push(stack, &context);
#else
    char *span_id = NULL;
    build_span_id_random(&span_id, NULL, 0);
    mo_stack_push(stack, &span_id);
#endif
}
/* }}} */

/* {{{ push span context with id */
void push_span_context_with_id(mo_stack *stack, char *span_id)
{
#ifdef USE_LEVEL_ID
    mo_span_context context;
    if (!mo_stack_empty(stack)) {
        mo_span_context *parent_node = (mo_span_context *)mo_stack_top(stack);
        parent_node->span_count++;
    }
    context.span_id = span_id;
    context.span_count = 1;
    mo_stack_push(stack, &context);
#else
    char *tmp_span_id = estrdup(span_id);
    mo_stack_push(stack, &tmp_span_id);
#endif
}
/* }}} */

/* {{{ pop span context */
void pop_span_context(mo_stack *stack)
{
    mo_stack_pop(stack, NULL);
}
/* }}} */

/* {{{ retrieve span id */
void retrieve_span_id(mo_stack *stack, char **span_id)
{
#ifdef USE_LEVEL_ID
    mo_span_context *context = (mo_span_context *) mo_stack_top(stack);
    if (context == NULL) {
        *span_id = NULL;
    } else {
        *span_id = context->span_id;
    }
#else
    char **sid = (char **)mo_stack_top(stack);
    if (sid == NULL) {
        *span_id = NULL;
    } else {
        *span_id = *sid;
    }
#endif
}
/* }}} */

/* {{{ retrieve parent span id */
void retrieve_parent_span_id(mo_stack *stack, char **parent_span_id)
{
#ifdef USE_LEVEL_ID
    mo_span_context *context = (mo_span_context *) mo_stack_sec_element(stack);
   if (context == NULL) {
        *parent_span_id = NULL;
   } else {
        *parent_span_id = context->span_id;
   }
#else
    char **psid = (char **)mo_stack_sec_element(stack);
    if (psid == NULL) {
        *parent_span_id = NULL;
    } else {
        *parent_span_id = *psid;
    }
#endif
}
/* }}} */

/* {{{ destroy all span context */
void destroy_span_context(mo_stack *stack)
{
    mo_stack_destroy(stack);
}
/* }}} */

void retrieve_span_id_4_frame(mo_frame_t *frame, char **span_id)
{
    retrieve_span_id(frame->span_stack, span_id);
}

void retrieve_parent_span_id_4_frame(mo_frame_t *frame, char **parent_span_id)
{
    retrieve_parent_span_id(frame->span_stack, parent_span_id);
}

/* {{{ build zipkin format main span */
void zn_start_span(zval **span, char *trace_id, char *server_name, char *span_id, char *parent_id, uint64_t timestamp, long duration)
{
    MO_ALLOC_INIT_ZVAL(*span);
    array_init(*span);
    mo_add_assoc_string(*span, "traceId", trace_id, 1);
    mo_add_assoc_string(*span, "name", server_name, 1);
    mo_add_assoc_string(*span, "version", SPAN_VERSION, 1);
    mo_add_assoc_string(*span, "id", span_id, 1);
    if (parent_id != NULL) {
        mo_add_assoc_string(*span, "parentId", parent_id, 1);
    }
    add_assoc_double(*span, "timestamp", timestamp);
    add_assoc_long(*span, "duration", duration);

    /* add annotions */
    zval *annotations;
    MO_ALLOC_INIT_ZVAL(annotations);
    array_init(annotations);
    add_assoc_zval(*span, "annotations", annotations);

    /* add binaryAnnotationss */
    zval *bannotations;
    MO_ALLOC_INIT_ZVAL(bannotations);
    array_init(bannotations);
    add_assoc_zval(*span, "binaryAnnotations", bannotations);

    MO_FREE_ALLOC_ZVAL(annotations);
    MO_FREE_ALLOC_ZVAL(bannotations);
}
/* }}} */

/* {{{ build zipkin service name */
char *zn_build_service_name(struct mo_chain_st *pct, char *service_name)
{
    char *g_service_name = pct->service_name;
    int service_len = strlen(service_name) + strlen(g_service_name) + 3;
    char *full_service_name = emalloc(service_len);
    memset(full_service_name, 0x00, service_len);
    snprintf(full_service_name, service_len, "%s-%s", g_service_name, service_name);
    full_service_name[service_len-1] = '\0';
    return full_service_name;
}
/* }}} */

/* {{{ add endpoint */
void zn_add_endpoint(zval *annotation, char *service_name, char *ipv4, long port)
{
    zval *endpoint;
    MO_ALLOC_INIT_ZVAL(endpoint);
    array_init(endpoint);
    mo_add_assoc_string(endpoint, "serviceName", service_name, 1);
    mo_add_assoc_string(endpoint, "ipv4", ipv4, 1);
    if (port != 0) {
        add_assoc_long(endpoint, "port", port);
    }
    add_assoc_zval(annotation, "endpoint", endpoint);
    MO_FREE_ALLOC_ZVAL(endpoint);
}
/* }}} */

/* {{{ add span annotation */
void zn_add_span_annotation(zval *span, const char *value, uint64_t timestamp, char *service_name, char *ipv4, long port)
{
    if (span == NULL || value == NULL || service_name == NULL || ipv4 == NULL) {
        return;
    }

    zval *annotations;
    if (mo_zend_hash_zval_find(Z_ARRVAL_P(span), "annotations", sizeof("annotations"), (void **)&annotations) == FAILURE) {
        return;
    }

    zval *annotation;
    MO_ALLOC_INIT_ZVAL(annotation);
    array_init(annotation);
    mo_add_assoc_string(annotation, "value", (char *)value, 1);
    add_assoc_double(annotation, "timestamp", timestamp);
    zn_add_endpoint(annotation, service_name, ipv4, port);
    add_next_index_zval(annotations, annotation);
    MO_FREE_ALLOC_ZVAL(annotation);

}
/* }}} */

/* {{{ add span annotation ex */
void zn_add_span_annotation_ex(zval *span, const char *value, uint64_t timestamp, struct mo_chain_st *pct)
{
    zn_add_span_annotation(span, value, timestamp, pct->service_name, pct->pch.ip, pct->pch.port);
}
/* }}} */

/* {{{ add span binnary annotation */
void zn_add_span_bannotation(zval *span, const char *key, const char *value, char *service_name, char *ipv4, long port)
{
    if (span == NULL || key == NULL || value == NULL || service_name == NULL || ipv4 == NULL) {
        return;
    }

    int init = 0;
    zval *bannotations;
    if (mo_zend_hash_zval_find(Z_ARRVAL_P(span), "binaryAnnotations", sizeof("binaryAnnotations"), (void **)&bannotations) == FAILURE) {
        /* add binaryAnnotationss */
        MO_ALLOC_INIT_ZVAL(bannotations);
        array_init(bannotations);
        add_assoc_zval(span, "binaryAnnotations", bannotations);
        init = 1;
    }

    zval *bannotation;
    MO_ALLOC_INIT_ZVAL(bannotation);
    array_init(bannotation);
    mo_add_assoc_string(bannotation, "key", (char *)key, 1);
    mo_add_assoc_string(bannotation, "value", (char *)value, 1);
    zn_add_endpoint(bannotation, service_name, ipv4, port);
    add_next_index_zval(bannotations, bannotation);
    MO_FREE_ALLOC_ZVAL(bannotation);

    if (init == 1) {
        MO_FREE_ALLOC_ZVAL(bannotations);
    }
}
/* }}} */

/* {{{ add span binnary annotation ex */
void zn_add_span_bannotation_ex(zval *span, const char *key, const char *value, struct mo_chain_st *pct)
{
    zn_add_span_bannotation(span, key, value, pct->service_name, pct->pch.ip, pct->pch.port);
}

/* {{{ build opentracing format span                            */
/* --------------------span format----------------------------- */
/* ------------------------------------------------------------ */
/* |operationName(string)|startTime(long)|finishTime(long)  |   */
/* |spanContext(map){traceID, spanID, parentSpanID}         |   */
/* |tags(map)|logs(list)|references(not used)               |   */
/* ------------------------------------------------------------ */
void ot_start_span(zval **span, char *op_name, char *trace_id, char *span_id, char *parent_id, int sampled, uint64_t start_time, uint64_t finish_time)
{
    MO_ALLOC_INIT_ZVAL(*span);
    array_init(*span);

    mo_add_assoc_string(*span, "operationName", op_name, 1);
    add_assoc_double(*span, "startTime", start_time);
    add_assoc_double(*span, "finishTime", finish_time);

    /* add spanContext */
    zval *spanContext;
    MO_ALLOC_INIT_ZVAL(spanContext);
    array_init(spanContext);
    mo_add_assoc_string(spanContext, "traceID", trace_id, 1);
    mo_add_assoc_string(spanContext, "spanID", span_id, 1);
    if (parent_id != NULL) {
        mo_add_assoc_string(spanContext, "parentSpanID", parent_id, 1);
    }
    add_assoc_zval(*span, "spanContext", spanContext);

    /* add tags */
    zval *tags;
    MO_ALLOC_INIT_ZVAL(tags);
    array_init(tags);
    add_assoc_zval(*span, "tags", tags);

    /* add logs */
    zval *logs;
    MO_ALLOC_INIT_ZVAL(logs);
    array_init(logs);
    add_assoc_zval(*span, "logs", logs);

    /* free map */
    MO_FREE_ALLOC_ZVAL(logs);
    MO_FREE_ALLOC_ZVAL(tags);
    MO_FREE_ALLOC_ZVAL(spanContext);
}
/* }}} */

/* {{{ opentracing add tag */
/* the tag list @see https://github.com/opentracing-contrib/opentracing-specification-zh/blob/master/semantic_conventions.md */
void ot_add_tag(zval *span, const char *key, const char *val)
{
    if (span == NULL || key == NULL || val == NULL ) {
        return;
    }
    zval *tags;
    if (mo_zend_hash_zval_find(Z_ARRVAL_P(span), "tags", sizeof("tags"), (void **)&tags) == FAILURE) {
        return;
    }
    mo_add_assoc_string(tags, key, (char *)val, 1);
}
/* }}} */

void ot_add_tag_long(zval *span, const char *key, long val)
{
    if (span == NULL || key == NULL) {
        return;
    }

    zval *tags;
    if (mo_zend_hash_zval_find(Z_ARRVAL_P(span), "tags", sizeof("tags"), (void **)&tags) == FAILURE) {
        return;
    }
    add_assoc_long(tags, key, val);
}

void ot_add_tag_bool(zval *span, const char *key, uint8_t val)
{
    if (span == NULL || key == NULL) {
        return;
    }

    zval *tags;
    if (mo_zend_hash_zval_find(Z_ARRVAL_P(span), "tags", sizeof("tags"), (void **)&tags) == FAILURE) {
        return;
    }
    add_assoc_bool(tags, key, val);
}

/* {{{ opentracing add log */
void ot_add_log(zval *span, uint64_t timestamp, int8_t field_num, ...)
{
    if (span == NULL) {
        return;
    }

    zval *logs;
    if (mo_zend_hash_zval_find(Z_ARRVAL_P(span), "logs", sizeof("logs"), (void **)&logs) == FAILURE) {
        return;
    }

    /* build fields */
    zval *fields;
    MO_ALLOC_INIT_ZVAL(fields);
    array_init(fields);

    va_list arg_ptr;
    int i = 0;
    char *key, *val = NULL;
    va_start(arg_ptr, field_num);

    /* fetch very key and val */
    for (;i < field_num; i++) {
        key = va_arg(arg_ptr, char*);
        val = va_arg(arg_ptr, char*);
        mo_add_assoc_string(fields, key, val, 1);
    }

    /* build log */
    zval *log;
    MO_ALLOC_INIT_ZVAL(log);
    array_init(log);
    add_assoc_double(log, "timestamp", timestamp);
    add_assoc_zval(log, "fields", fields);

    /* add log */
    add_next_index_zval(logs, log);

    /* free map */
    MO_FREE_ALLOC_ZVAL(log);
    MO_FREE_ALLOC_ZVAL(fields);
}

/* span function wrapper for zipkin */
void zn_start_span_builder(zval **span, char *service_name, char *trace_id, char *span_id, char *parent_id, uint64_t start_time, uint64_t finish_time, struct mo_chain_st *pct, uint8_t an_type)
{
    zn_start_span(span, trace_id, service_name, span_id, parent_id, start_time, finish_time - start_time);
    if (an_type == AN_SERVER) {
        zn_add_span_annotation_ex(*span, "sr", start_time, pct);
        zn_add_span_annotation_ex(*span, "ss", finish_time, pct);
    } else {
        zn_add_span_annotation_ex(*span, "cs", start_time, pct);
        zn_add_span_annotation_ex(*span, "cr", finish_time, pct);
    }
}

void zn_start_span_ex_builder(zval **span, char *service_name, struct mo_chain_st *pct, mo_frame_t *frame, uint8_t an_type)
{
    char *span_id;
    char *parent_span_id;

    retrieve_span_id_4_frame(frame, &span_id);
    retrieve_parent_span_id_4_frame(frame, &parent_span_id);

    zn_start_span_builder(span, service_name, pct->pch.trace_id->val, span_id, parent_span_id, frame->entry_time, frame->exit_time, pct, an_type);
}

void zn_add_segments_builder(zval **segments, zval *span, struct mo_chain_st *pct) {

}

void zn_span_add_ba_builder(zval *span, const char *key, const char *value, uint64_t timestamp, char *service_name, char *ipv4, long port, uint8_t ba_type)
{
    zn_add_span_bannotation(span, key, value, service_name, ipv4, port);
}

void zn_span_add_ba_ex_builder(zval *span, const char *key, const char *value, uint64_t timestamp, struct mo_chain_st *pct, uint8_t ba_type)
{
    zn_span_add_ba_builder(span, key, value, timestamp, pct->service_name, pct->pch.ip, pct->pch.port, ba_type);
}

/** span function wrapper for opentracing */
void ot_start_span_builder(zval **span, char *service_name, char *trace_id, char *span_id, char *parent_id, uint64_t start_time, uint64_t finish_time, struct mo_chain_st *pct, uint8_t an_type)
{
    ot_start_span(span, service_name, trace_id, span_id, parent_id, 1, start_time, finish_time);
    if (an_type == AN_SERVER) {
        ot_add_tag(*span, "span.kind", "server");
    } else {
        ot_add_tag(*span, "span.kind", "client");
    }
}

void ot_start_span_ex_builder(zval **span, char *service_name, struct mo_chain_st *pct, mo_frame_t *frame, uint8_t an_type)
{
    char *span_id;
    char *parent_span_id;

    retrieve_span_id_4_frame(frame, &span_id);
    retrieve_parent_span_id_4_frame(frame, &parent_span_id);

    ot_start_span_builder(span, service_name, pct->pch.trace_id->val, span_id, parent_span_id, frame->entry_time, frame->exit_time, pct, an_type);
}

void ot_add_segments_builder(zval **segments, zval *span, struct mo_chain_st *pct) {

}

void ot_span_add_ba_builder(zval *span, const char *key, const char *value, uint64_t timestamp, char *service_name, char *ipv4, long port, uint8_t ba_type)
{
    switch (ba_type) {
        case BA_NORMAL:
            ot_add_tag(span, key, value);
            break;
        case BA_SA:
            ot_add_tag(span, "peer.ipv4", ipv4);
            ot_add_tag_long(span, "peer.port", port);
            ot_add_tag(span, "peer.service", service_name);
            break;
        case BA_SA_HOST:
            ot_add_tag(span, "peer.hostname", ipv4);
            ot_add_tag_long(span, "peer.port", port);
            ot_add_tag(span, "peer.service", service_name);
            break;
        case BA_SA_IP:
            ot_add_tag(span, "peer.ipv4", ipv4);
            ot_add_tag_long(span, "peer.port", port);
            ot_add_tag(span, "peer.service", service_name);
            break;
        case BA_SA_DSN:
            ot_add_tag(span, "peer.address", value);
            break;
        case BA_PATH:
            /* not use for opentracing */
            break;
        case BA_ERROR:
            ot_add_tag_bool(span, "error", 1);
            ot_add_log(span, timestamp, 3, "event", "error", "error.kind", "Exception", "message", value);
        default:
            break;
    }

}
void ot_span_add_ba_ex_builder(zval *span, const char *key, const char *value, uint64_t timestamp, struct mo_chain_st *pct, uint8_t ba_type)
{
    ot_span_add_ba_builder(span, key, value, timestamp, pct->service_name, pct->pch.ip, pct->pch.port, ba_type);
}

/* {{{ build skywalking format span  */
/*                                    */
/*           **      **              */
/*               *                    */
/* * * * * * * * * * * * * * * * * *  */

/* {{{ skywalking add log */
void sk_add_log(zval *span, uint64_t timestamp, int8_t field_num, ...) {

    if (span == NULL) {
        return;
    }

    //traversing the array
    zval *logs, *logs_tmp, *log_filed, *log_filed_tmp;

    if (mo_zend_hash_zval_find(Z_ARRVAL_P(span), "lo", sizeof("lo"), (void **)&logs) == FAILURE) {
        return;
    }

    MO_ALLOC_INIT_ZVAL(logs_tmp);
    array_init(logs_tmp);

    MO_ALLOC_INIT_ZVAL(log_filed);
    array_init(log_filed);

    add_assoc_double(logs_tmp, "ti", timestamp);
    add_assoc_zval(logs_tmp, "ld", log_filed);

    add_next_index_zval(logs, logs_tmp);

    /* build fields */
    va_list arg_ptr;
    va_start(arg_ptr, field_num);

    /* fetch very key and val */
    int i;
    for (i = 0; i < field_num; i++) {
        MO_ALLOC_INIT_ZVAL(log_filed_tmp);
        array_init(log_filed_tmp);

        mo_add_assoc_string(log_filed_tmp, "k", (char *)va_arg(arg_ptr, char*), 1);
        mo_add_assoc_string(log_filed_tmp, "v", (char *)va_arg(arg_ptr, char*), 1);

        add_next_index_zval(log_filed, log_filed_tmp);

        MO_FREE_ALLOC_ZVAL(log_filed_tmp);
    }

    MO_FREE_ALLOC_ZVAL(log_filed);
    MO_FREE_ALLOC_ZVAL(logs_tmp);
}

/* {{{ skywalking add tag */
/* the tag list @see https://github.com/opentracing-contrib/opentracing-specification-zh/blob/master/semantic_conventions.md */
void sk_add_tag(zval *span, const char *key, const char *val)
{
    if (span == NULL || key == NULL || val == NULL) {
        return;
    }

    zval *tags, *tags_tmp;
    if (mo_zend_hash_zval_find(Z_ARRVAL_P(span), "to", sizeof("to"), (void **) &tags) == FAILURE) {
        return;
    }

    MO_ALLOC_INIT_ZVAL(tags_tmp);
    array_init(tags_tmp);

    mo_add_assoc_string(tags_tmp, "k", (char *)key, 1);
    mo_add_assoc_string(tags_tmp, "v", (char *)val, 1);
    add_next_index_zval(tags, tags_tmp);

    MO_FREE_ALLOC_ZVAL(tags_tmp);
}

/* add parent span object */
void sk_add_parent_span(zval *span, char *op_name, char *trace_id, char *span_id, char *parent_id, int sampled,
                        uint64_t start_time, uint64_t finish_time, struct mo_chain_st *pct) {
    if (span == NULL) {
        return;
    }
    zval *segment_ref, *segment_ref_tmp;
    if (mo_zend_hash_zval_find(Z_ARRVAL_P(span), "rs", sizeof("rs"), (void **)&segment_ref) == FAILURE) {
        return;
    }

    MO_ALLOC_INIT_ZVAL(segment_ref_tmp);
    array_init(segment_ref_tmp);

    mo_add_assoc_string(segment_ref_tmp, "pts", "", 1); //parentTraceSegmentId,上级的segment_id 一个应用中的一个实例在链路中产生的编号
    add_assoc_long(segment_ref_tmp, "ppi", 2); //parentApplicationInstanceId
    add_assoc_long(segment_ref_tmp, "psp", 1); //parentSpanId
    add_assoc_long(segment_ref_tmp, "psi", 0); //parentServiceId,上级的服务编号(服务注册后的ID)
    mo_add_assoc_string(segment_ref_tmp, "psn", "/www/data/php_service/test.php", 1); //parentServiceName, 上级的服务名
    add_assoc_long(segment_ref_tmp, "ni", 0); //networkAddressId, 上级调用时使用的地址注册后的ID
    mo_add_assoc_string(segment_ref_tmp, "nn", "172.25.0.4:20880", 1); //networkAddress, 上级的地址
    add_assoc_long(segment_ref_tmp, "eii", 2); //entryApplicationInstanceId, 入口的实例编号
    add_assoc_long(segment_ref_tmp, "esi", 0); //entryServiceId, 入口的服务编号
    mo_add_assoc_string(segment_ref_tmp, "esn", "", 1); //entryServiceName, 入口的服务名词
    add_assoc_long(segment_ref_tmp, "rv", 0); //RefTypeValue, 调用方式（CrossProcess，CrossThread）

    add_next_index_zval(segment_ref, segment_ref_tmp);

    MO_FREE_ALLOC_ZVAL(segment_ref_tmp);
}

/* add span object */
void sk_add_span(zval **span, char *op_name, char *trace_id, char *span_id, char *parent_id, int sampled,
                 uint64_t start_time, uint64_t finish_time, struct mo_chain_st *pct) {

    MO_ALLOC_INIT_ZVAL(*span);
    array_init(*span);

    unsigned int spanId;
    sscanf(span_id, "%d", &spanId);

    //add_assoc_long(*span, "si", span_id); //spanId
    add_assoc_long(*span, "si", spanId); //spanId
    add_assoc_long(*span, "tv", pct->span_type); //spanType  -1 Unknown, 0 Entry, 1 Exit, 2 Local
    add_assoc_long(*span, "lv", pct->span_layer); //spanLayer 0 Unknown, 1 Database, 2 RPC, 3 Http, 4 MQ, 5 Cache, -1 UNRECOGNIZED
    add_assoc_double(*span, "st", start_time); //startTime
    add_assoc_double(*span, "et", finish_time); //endTime
    add_assoc_long(*span, "ci", pct->component_id == 0 ? PHP_CLI : pct->component_id); //componentId 2:http_client

    char on[128];
    sprintf(on, "%s/%s_%s", pct->service_name, op_name, span_id);
    mo_add_assoc_string(*span, "cn", op_name, 1); //componentName pct->script

    //add_assoc_long(*span, "oi", 0); //operationNanmeId
    mo_add_assoc_string(*span, "on", on, 1); //operationNanme
    //add_assoc_long(*span, "pi", 0); //operationNanmeId
    if (pct->error_list != NULL) {
        add_assoc_bool(*span, "ie", 0); //isError
    } else {
        add_assoc_bool(*span, "ie", 1); //isError
    }

    //add_assoc_string(*span, "pn", pct->pch.ip); //peerName

    /* add trace segment reference */
    zval *segment_ref;
    if (parent_id == NULL) {
        add_assoc_long(*span, "ps", -1); //parentSpanId
    } else {
        unsigned int parentId;
        sscanf(parent_id, "%d", &parentId);
        add_assoc_long(*span, "ps", parentId); //parentSpanId

        //MO_ALLOC_INIT_ZVAL(segment_ref);
        //array_init(segment_ref);
        //add_assoc_zval(*span, "rs", segment_ref);
        //MO_FREE_ALLOC_ZVAL(segment_ref);
    }

    /* add tags */
    zval *tags;
    MO_ALLOC_INIT_ZVAL(tags);
    array_init(tags);
    add_assoc_zval(*span, "to", tags);

    /* add logs */
    zval *logs;
    MO_ALLOC_INIT_ZVAL(logs);
    array_init(logs);
    add_assoc_zval(*span, "lo", logs);

    /* free map */
    MO_FREE_ALLOC_ZVAL(logs);
    MO_FREE_ALLOC_ZVAL(tags);
}


/**
 * report segments data to skywalking collector
 * @param application_id
 * @param instance_id
 */
void sk_add_segments_builder(zval **segments, zval *span, struct mo_chain_st *pct) {

    MO_ALLOC_INIT_ZVAL(*segments);
    array_init(*segments);

    /* add global trace id */
    zval *segments_tmp, *globalTraceIds, *traceId;
    MO_ALLOC_INIT_ZVAL(segments_tmp);
    MO_ALLOC_INIT_ZVAL(globalTraceIds);
    MO_ALLOC_INIT_ZVAL(traceId);
    array_init(segments_tmp);
    array_init(globalTraceIds);
    array_init(traceId);

    add_next_index_long(traceId, pct->instance_id); //applicationInstanceId
    add_next_index_long(traceId, current_thread_pid()); //pid
    add_next_index_double(traceId, current_system_time_millis());
    add_next_index_zval(globalTraceIds, traceId);

    add_assoc_zval(segments_tmp, "gt", globalTraceIds); //globalTraceIds 链路编码，与调用方相同

    /* add trace segment object */
    zval *segmentObject;
    MO_ALLOC_INIT_ZVAL(segmentObject);
    array_init(segmentObject);

    add_assoc_zval(segmentObject, "ts", traceId); //traceSegmentId，新产生
    add_assoc_long(segmentObject, "ai", pct->application_id);
    add_assoc_long(segmentObject, "ii", pct->instance_id);
    add_assoc_zval(segments_tmp, "sg", segmentObject);

    /* add span object */
    add_assoc_zval(segmentObject, "ss", span);

    add_next_index_zval(*segments, segments_tmp);

    MO_FREE_ALLOC_ZVAL(segments_tmp);
    MO_FREE_ALLOC_ZVAL(globalTraceIds);
    MO_FREE_ALLOC_ZVAL(traceId);
    MO_FREE_ALLOC_ZVAL(segmentObject);
}

void sk_register_service_builder(char *res_data, int application_id, char *agent_uuid) {
    cJSON *instanceSpan = cJSON_CreateObject();
    cJSON_AddNumberToObject(instanceSpan, "ai", application_id);
    cJSON_AddStringToObject(instanceSpan, "au", agent_uuid);
    cJSON_AddNumberToObject(instanceSpan, "rt", current_system_time_millis());

    cJSON *osInfo = cJSON_CreateObject();
    cJSON_AddStringToObject(osInfo, "osName", current_os_name());
    cJSON_AddStringToObject(osInfo, "hostName", current_host_name());
    cJSON_AddNumberToObject(osInfo, "processId", current_thread_pid());

    cJSON *ipv4s = NULL;
    cJSON_AddItemToObject(osInfo, "ipv4s", ipv4s = cJSON_CreateArray());

    char **ipv4 = current_ipv4();
    int len = sizeof(ipv4) / sizeof(ipv4[0]);
    int i;
    for (i = 0; i < len; i++) {
        char *string = ipv4[i];
        char result1[64] = "";
        strcpy(result1, string);
        cJSON_AddItemToArray(ipv4s, cJSON_CreateString(result1));
    }

    cJSON_AddItemToObject(instanceSpan, "oi", osInfo);

    //将json结构格式化到缓冲区
    strcpy(res_data, cJSON_Print(instanceSpan));
}

/** span function wrapper for opentracing */
void sk_start_span_builder(zval **span, char *service_name, char *trace_id, char *span_id, char *parent_id, uint64_t start_time, uint64_t finish_time, struct mo_chain_st *pct, uint8_t an_type)
{
    sk_add_span(span, service_name, trace_id, span_id, parent_id, 1, start_time, finish_time, pct);
    //sk_add_parent_span(*span, service_name, trace_id, span_id, parent_id, 1, start_time, finish_time, pct);

    if (an_type == AN_SERVER) {
        sk_add_tag(*span, "span.kind", "server");
    } else {
        sk_add_tag(*span, "span.kind", "client");
    }
}

void sk_start_span_ex_builder(zval **span, char *service_name, struct mo_chain_st *pct, mo_frame_t *frame, uint8_t an_type)
{
    char *span_id;
    char *parent_span_id;

    retrieve_span_id_4_frame(frame, &span_id);
    retrieve_parent_span_id_4_frame(frame, &parent_span_id);

    sk_start_span_builder(span, service_name, pct->pch.trace_id->val, span_id, parent_span_id, frame->entry_time, frame->exit_time, pct, an_type);
}

void sk_span_add_ba_builder(zval *span, const char *key, const char *value, uint64_t timestamp, char *service_name, char *ipv4, long port, uint8_t ba_type)
{

    if (span == NULL || key == NULL || value == NULL) {
        return ;
    }

    char str_port[64];
    sprintf(str_port, "%ld", port);

    char peer_name[64];
    sprintf(peer_name, "%s:%s", ipv4, str_port);

    if (strlen(value) > 128) {
        char tmp[128];
        strncpy(tmp, value, 128);
        value = tmp;
    }

    switch (ba_type) {
        case BA_NORMAL:
            sk_add_tag(span, key, value);
            break;
        case BA_SA:
            mo_add_assoc_string(span, "pn", peer_name, 1); //peerName
            sk_add_tag(span, "peer.ipv4", ipv4);
            sk_add_tag(span, "peer.port", str_port);
            sk_add_tag(span, "peer.service", service_name);
            break;
        case BA_SA_HOST:
            mo_add_assoc_string(span, "pn", peer_name, 1); //peerName
            sk_add_tag(span, "peer.hostname", ipv4);
            sk_add_tag(span, "peer.port", str_port);
            sk_add_tag(span, "peer.service", service_name);
            break;
        case BA_SA_IP:
            mo_add_assoc_string(span, "pn", peer_name, 1); //peerName
            sk_add_tag(span, "peer.ipv4", ipv4);
            sk_add_tag(span, "peer.port", str_port);
            sk_add_tag(span, "peer.service", service_name);
            break;
        case BA_SA_DSN:
            sk_add_tag(span, "peer.address", value);
            break;
        case BA_PATH:
            /* not use for opentracing */
            sk_add_tag(span, key, value);
            break;
        case BA_ERROR:
            sk_add_tag(span, "error", "true");
            add_assoc_bool(span, "ie", 1);
            sk_add_log(span, timestamp, 3, "event", "error", "error.kind", "Exception", "message", value);
        default:
            break;
    }

}

void sk_span_add_ba_ex_builder(zval *span, const char *key, const char *value, uint64_t timestamp, struct mo_chain_st *pct, uint8_t ba_type)
{
    sk_span_add_ba_builder(span, key, value, timestamp, pct->service_name, pct->pch.ip, pct->pch.port, ba_type);
}

void mo_span_pre_init_ctor(mo_span_builder *psb, struct mo_chain_st *pct, char* sink_http_uri, char* service_name) {

    if (psb->type == SKYWALKING) {
        //char *server_url = sink_http_uri;
        //char server_url[32] = "";

        char *server_url = emalloc(sizeof(char) * 64);
        memset(server_url, 0x00, sizeof (char) * 64);
        strcpy(server_url, sink_http_uri);

        char* url = sk_get_server(server_url);
        if (url == NULL) {
            return ;
        }

        pct->server_url = server_url;
        memset(sink_http_uri, 0x00, sizeof(sink_http_uri));
        strcat(strcat(sink_http_uri, url), SK_SEGMENTS);

        //register application
        int application_id = sk_register_application(service_name, url);
        //第一次注册返回的是0，第二次注册才会返回真正的值
        while (application_id == 0) {
            sleep(1);
            application_id = sk_register_application(service_name, url);
        }
        pct->application_id = application_id;
        SLOG(SLOG_INFO, "molten register application id [%d]", application_id);

        //register instance
        int instance_id = sk_register_instance(application_id, url);
        pct->instance_id = instance_id;
        SLOG(SLOG_INFO, "molten register instance id [%d]", instance_id);

        //create thread to listener heart beat
        sk_instance_heartbeat(pct);

        //efree(server_url);
    }
}

/**
 * get all address from the apm collector server
 * @param url   get address url
 * @return  all address
 */
char *sk_get_server(char *url) {
    //get the apm server address
    char response[256];
    CURLcode lcode = get_request(url, response);
    if (lcode != CURLE_OK || response == NULL) {
        return NULL;
    }

    //parse josn string to json object
    cJSON *pJSON = cJSON_Parse(response);
    //get the josn array size
    int size = cJSON_GetArraySize(pJSON);
    if (size <= 0) {
        return NULL;
    }

    char **servers = emalloc(sizeof(char) * 64);
    int i;
    for (i = 0; i < size; i++) {
        servers[i] = cJSON_GetArrayItem(pJSON, i)->valuestring;
    }

    /* init load balancer */
    server *ss = initDefaultServers(servers, size);
    int index = getNextServerIndex(ss, size);

    if (servers != NULL) {
        efree(servers);
    }

    return ss[index].name;
}

/**
 * register application to skywalking server
 * @param name application name
 * @param server_url register url
 * @return application id
 */
int sk_register_application(char *name, char *server_url) {

    //parse register url
    char url[64] = "";
    strcat(strcat(url, server_url), SK_REGISTER_APPLICATION);

    //parse register application data, eg: "[\"php_service_01\"]";
    char post_data[64] = "[";
    strcat(strcat(post_data, name), "]");

    char register_application_response[128] = "";
    SLOG(SLOG_INFO, "molten register service, url:[%s], application name:[%s]", url, name);

    CURLcode curLcode = post_request(url, post_data, register_application_response);
    if (curLcode != CURLE_OK || register_application_response == "") {
        SLOG(SLOG_ERROR, "molten register service has error ret code [%d] response [%s]", curLcode, register_application_response);
        return 0;
    }
    SLOG(SLOG_INFO, "molten register application [%s] result [%s]", url, register_application_response);

    cJSON *pRegisterJSON = cJSON_Parse(register_application_response);
    cJSON *applicationJson = cJSON_GetArrayItem(pRegisterJSON, 0);
    cJSON *applicationIdJson = cJSON_GetObjectItem(applicationJson, "i");
    int application_id =  applicationIdJson->valueint;

    cJSON_Delete(applicationJson);  //释放内存

    return application_id;
}

void register_heartbeat_callback(struct mo_chain_st *pct) {
    for (;;) {
        if (pct->server_url == NULL || strlen(pct->server_url) > sizeof(char) * 64) {
            return ;
        }
        php_printf("This is a pthread.  instance_id: %d, server_url: %s, size:%d\n", pct->instance_id, pct->server_url, strlen(pct->server_url));
        SLOG(SLOG_INFO, "This is a pthread.  instance_id: %d, server_url: %s, size:%d", pct->instance_id, pct->server_url, strlen(pct->server_url));
        //register heart beat

        php_sleep(1);
    }
}

void sk_instance_heartbeat(struct mo_chain_st *pct) {
    pthread_t id;
    int ret;
    ret = pthread_create(&id, NULL, (void *) register_heartbeat_callback, pct); // 传入多个参数，使用结构体
    if (ret != 0) {
        printf("Create pthread error!\n");
        exit(1);
    }
}

int get_instance_id(char *response_data) {
    cJSON *pRegisterJSON = cJSON_Parse(response_data);
    cJSON *instanceIdJson = cJSON_GetObjectItem(pRegisterJSON, "ii");
    cJSON_Delete(pRegisterJSON);  //释放内存
    return instanceIdJson->valueint;
}

/**
 * register instance
 * request data eg:
 * {
 *      "ai": -1,
 *		"au": "125456ss4sgeessfgsf665111",
 *		"rt": 1531298394832,
 *		"oi": {
 *			"osName": "Windows 7",
 *			"hostname": "3F-tangzhongyuan",
 *			"processNo": 21780,
 *			"ipv4s": ["10.32.5.143"]
 *		}
 *	}
 *
 * @param application_id
 * @return  instance id
 *
 */
int sk_register_instance(int application_id, char *server_url) {
    //parse register url
    char url[64] = "";
    strcat(strcat(url, server_url), SK_REGISTER_SERVICE);

    char res_data[256] = "";
    char *agentUUId[] = {};
    rand64hex(agentUUId);
    sk_register_service_builder(res_data, application_id, agentUUId[0]);

    SLOG(SLOG_INFO, "molten register instance, url:[%s], post_data:[%s]", url, res_data);

    char response_data[32];
    CURLcode curLcode = post_request(url, res_data, response_data);
    if (curLcode != CURLE_OK || response_data == "") {
        SLOG(SLOG_ERROR, "molten register instance has error ret code [%d] response [%s]", curLcode, response_data);
        return 0;
    }
    SLOG(SLOG_INFO, "molten register instance, url:[%s], response_data:[%s]", url, response_data);

    //get instance id from parse response
    int instance_id = get_instance_id(response_data);
    while (instance_id == 0) {
        sleep(1);
        post_request(url, res_data, response_data);
        instance_id = get_instance_id(response_data);
    }
    return instance_id;
}
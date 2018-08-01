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
#include <common/host_info.h>
#include "molten_span.h"
#include "common/molten_http_util.h"
#include "common/molten_cJSON.h"
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
void zn_start_span(zval **span, char *trace_id, char *server_name, char *span_id, char *parent_id, long timestamp, long duration)
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
    add_assoc_long(*span, "timestamp", timestamp);
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
void zn_add_span_annotation(zval *span, const char *value, long timestamp, char *service_name, char *ipv4, long port)
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
    add_assoc_long(annotation, "timestamp", timestamp);
    zn_add_endpoint(annotation, service_name, ipv4, port);
    add_next_index_zval(annotations, annotation);
    MO_FREE_ALLOC_ZVAL(annotation);

}
/* }}} */

/* {{{ add span annotation ex */
void zn_add_span_annotation_ex(zval *span, const char *value, long timestamp, struct mo_chain_st *pct)
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
void ot_start_span(zval **span, char *op_name, char *trace_id, char *span_id, char *parent_id, int sampled, long start_time, long finish_time)
{
    MO_ALLOC_INIT_ZVAL(*span);
    array_init(*span);

    mo_add_assoc_string(*span, "operationName", op_name, 1);
    add_assoc_long(*span, "startTime", start_time);
    add_assoc_long(*span, "finishTime", finish_time);

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
void ot_add_log(zval *span, long timestamp, int8_t field_num, ...)
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
    add_assoc_long(log, "timestamp", timestamp);
    add_assoc_zval(log, "fields", fields);

    /* add log */
    add_next_index_zval(logs, log);

    /* free map */
    MO_FREE_ALLOC_ZVAL(log);
    MO_FREE_ALLOC_ZVAL(fields);
}

/* span function wrapper for zipkin */
void zn_start_span_builder(zval **span, char *service_name, char *trace_id, char *span_id, char *parent_id, long start_time, long finish_time, struct mo_chain_st *pct, uint8_t an_type)
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

void zn_span_add_ba_builder(zval *span, const char *key, const char *value, long timestamp, char *service_name, char *ipv4, long port, uint8_t ba_type)
{
    zn_add_span_bannotation(span, key, value, service_name, ipv4, port);
}

void zn_span_add_ba_ex_builder(zval *span, const char *key, const char *value, long timestamp, struct mo_chain_st *pct, uint8_t ba_type)
{
    zn_span_add_ba_builder(span, key, value, timestamp, pct->service_name, pct->pch.ip, pct->pch.port, ba_type);
}

/** span function wrapper for opentracing */
void ot_start_span_builder(zval **span, char *service_name, char *trace_id, char *span_id, char *parent_id, long start_time, long finish_time, struct mo_chain_st *pct, uint8_t an_type)
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

void ot_span_add_ba_builder(zval *span, const char *key, const char *value, long timestamp, char *service_name, char *ipv4, long port, uint8_t ba_type)
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
void ot_span_add_ba_ex_builder(zval *span, const char *key, const char *value, long timestamp, struct mo_chain_st *pct, uint8_t ba_type)
{
    ot_span_add_ba_builder(span, key, value, timestamp, pct->service_name, pct->pch.ip, pct->pch.port, ba_type);
}

/* {{{ build opentracing format span  */
/*                                    */
/*           **      **              */
/*               *                    */
/* * * * * * * * * * * * * * * * * *  */
void sk_register_service_builder(char *res_data, int application_id, char *agent_uuid) {
    cJSON *instanceSpan = cJSON_CreateObject();
    cJSON_AddNumberToObject(instanceSpan, "ai", application_id);
    cJSON_AddStringToObject(instanceSpan, "au", agent_uuid);
    cJSON_AddNumberToObject(instanceSpan, "rt", current_system_time_millis());

    cJSON *osInfo = cJSON_CreateObject();
    cJSON_AddStringToObject(osInfo, "osName", current_os_name());
    cJSON_AddStringToObject(osInfo, "hostname", current_host_name());
    cJSON_AddNumberToObject(osInfo, "processNo", current_thread_pid());

    cJSON *ipv4s = NULL;
    cJSON_AddItemToObject(osInfo, "ipv4s", ipv4s = cJSON_CreateArray());

    char **ipv4 = current_ipv4();
    int len = sizeof(ipv4) / sizeof(ipv4[0]);
    for (int i = 0; i < len; i++) {
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
void sk_start_span_builder(zval **span, char *service_name, char *trace_id, char *span_id, char *parent_id, long start_time, long finish_time, struct mo_chain_st *pct, uint8_t an_type)
{
    sk_log_segments(span, service_name, trace_id, span_id, parent_id, 1, start_time, finish_time);
    //ot_start_span(span, service_name, trace_id, span_id, parent_id, 1, start_time, finish_time);
    if (an_type == AN_SERVER) {
        ot_add_tag(*span, "span.kind", "server");
    } else {
        ot_add_tag(*span, "span.kind", "client");
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

void sk_span_add_ba_builder(zval *span, const char *key, const char *value, long timestamp, char *service_name, char *ipv4, long port, uint8_t ba_type)
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

void sk_span_add_ba_ex_builder(zval *span, const char *key, const char *value, long timestamp, struct mo_chain_st *pct, uint8_t ba_type)
{
    ot_span_add_ba_builder(span, key, value, timestamp, pct->service_name, pct->pch.ip, pct->pch.port, ba_type);
}

void mo_span_pre_init_ctor(mo_span_builder *psb, char* sink_http_uri, char* service_name) {
    if (psb->type == SKYWALKING) {
        //char* server_url = "http://10.40.6.114:10800/agent/jetty";
        char *server_url = sink_http_uri;
        globale_init();
        char* url = sk_get_server(server_url);
        if (url == NULL) {
            return ;
        }
        //register application
        int application_id = sk_register_application(service_name, url);
        //第一次注册返回的是0，第二次注册才会返回真正的值
        while (application_id == 0) {
            sleep(1);
            application_id = sk_register_application(service_name, url);
        }
        psb->application_id = application_id;
        SLOG(SLOG_INFO, "molten register application id [%d]", application_id);

        //register instance
        int instance_id = sk_register_instance(application_id, url);
        psb->instance_id = instance_id;
        SLOG(SLOG_INFO, "molten register instance id [%d]", instance_id);
        //release curl
        globale_release();
    }
}

/**
 * get all address from the apm collector server
 * @param url   get address url
 * @return  all address
 */
char *sk_get_server(char *url) {
    //get the apm server address
    char response[128] = "";
    CURLcode lcode = get_request(url, response);
    if (lcode != CURLE_OK || response == "") {
        return NULL;
    }

    //parse josn string to json object
    cJSON *pJSON = cJSON_Parse(response);
    //get the josn array size
    int size = cJSON_GetArraySize(pJSON);
    if (size <= 0) {
        return NULL;
    }

    //get the first apm server address
    char *http_url = cJSON_GetArrayItem(pJSON, 0)->valuestring;

    static char result[32];
    strcpy(result, http_url);
    return result;
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

    //char response_data[32] = "";
    char *response_data = malloc(32);
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
    free(response_data);
    return instance_id;
}

/**
 * report segments data to skywalking collector
 * @param application_id
 * @param instance_id
 */
void sk_log_segments(zval **span, char *op_name, char *trace_id, char *span_id, char *parent_id, int sampled, long start_time, long finish_time) {

    MO_ALLOC_INIT_ZVAL(*span);
    array_init(*span);
    add_assoc_string(*span, "gt", ""); //globalTraceIds 链路编码，与调用方相同

    /* add trace segment object */
    zval *segmentObject;
    MO_ALLOC_INIT_ZVAL(segmentObject);
    array_init(segmentObject);
    add_assoc_string(segmentObject, "ts", ""); //traceSegmentId，新产生
    add_assoc_long(segmentObject, "ai", 0);
    add_assoc_long(segmentObject, "ii", 1);

    /* add span object */
    zval *spanObject;
    MO_ALLOC_INIT_ZVAL(spanObject);
    array_init(spanObject);
    add_assoc_long(spanObject, "si", 0); //spanId
    add_assoc_long(spanObject, "tv", 0); //spanType
    add_assoc_long(spanObject, "lv", 0); //spanLayer
    add_assoc_long(spanObject, "ps", 0); //parentSpanId
    add_assoc_long(spanObject, "st", start_time); //startTime
    add_assoc_long(spanObject, "et", finish_time); //endTime
    add_assoc_long(spanObject, "ci", 3); //componentId
    add_assoc_string(spanObject, "cn", ""); //componentName
    add_assoc_long(spanObject, "oi", 0); //operationNanmeId
    add_assoc_string(spanObject, "on", ""); //operationNanme
    add_assoc_long(spanObject, "pi", 0); //operationNanmeId
    add_assoc_string(spanObject, "pn", ""); //peerName
    add_assoc_bool(spanObject, "ie", 0); //isError

    /* add trace segement reference */
    zval *segementRef;
    MO_ALLOC_INIT_ZVAL(segementRef);
    //MAKE_STD_ZVAL(segementRef);
    array_init(segementRef);
    add_assoc_string(segementRef, "pts", ""); //parentTraceSegmentId,上级的segment_id 一个应用中的一个实例在链路中产生的编号
    add_assoc_long(segementRef, "ppi", 2); //parentApplicationInstanceId
    add_assoc_long(segementRef, "psp", 1); //parentSpanId
    add_assoc_long(segementRef, "psi", 0); //parentServiceId,上级的服务编号(服务注册后的ID)
    add_assoc_string(segementRef, "psn", "/www/data/php_service/test.php"); //parentServiceName, 上级的服务名
    add_assoc_long(segementRef, "ni", 0); //networkAddressId, 上级调用时使用的地址注册后的ID
    add_assoc_string(segementRef, "nn", "172.25.0.4:20880"); //networkAddress, 上级的地址
    add_assoc_long(segementRef, "eii", 2); //entryApplicationInstanceId, 入口的实例编号
    add_assoc_long(segementRef, "esi", 0); //entryServiceId, 入口的服务编号
    add_assoc_string(segementRef, "esn", ""); //entryServiceName, 入口的服务名词
    add_assoc_long(segementRef, "rv", 0); //RefTypeValue, 调用方式（CrossProcess，CrossThread）

    zval t_tags, t_tags_tmp;
    array_init(&t_tags);
    array_init(&t_tags_tmp);

    zval z_url_k, z_url_v;
    ZVAL_STRING(&z_url_k, "url");
    ZVAL_STRING(&z_url_v, "http://www.baidu.com");

    zend_hash_str_update(Z_ARRVAL(t_tags_tmp), "k", sizeof("k") - 1, &z_url_k);
    zend_hash_str_update(Z_ARRVAL(t_tags_tmp), "v", sizeof("v") - 1, &z_url_v);

    add_index_zval(&t_tags, 0, &t_tags_tmp);
    add_index_zval(&t_tags, 1, &t_tags_tmp);
    add_assoc_zval(spanObject, "qq", &t_tags);

    zval arry, array_tmp;
    ZVAL_LONG(&array_tmp, 1);
    ZVAL_LONG(&array_tmp, 3);
    ZVAL_NEW_ARR(&arry);
    add_index_zval(&arry, 0, &array_tmp);
    add_assoc_zval(spanObject, "qs", &arry);

    /* add tags */
    zval *tags;
    MO_ALLOC_INIT_ZVAL(tags);
    array_init(tags);
    add_assoc_zval(spanObject, "to", tags);

    /* add logs */
    zval *logs;
    MO_ALLOC_INIT_ZVAL(logs);
    array_init(logs);
    add_assoc_zval(spanObject, "lo", logs);

    add_assoc_zval(spanObject, "rs", segementRef);
    add_assoc_zval(segmentObject, "ss", spanObject);
    add_assoc_zval(*span, "sg", segmentObject);

    /* free map */
    MO_FREE_ALLOC_ZVAL(logs);
    MO_FREE_ALLOC_ZVAL(tags);
    MO_FREE_ALLOC_ZVAL(segementRef);
    MO_FREE_ALLOC_ZVAL(spanObject);
    MO_FREE_ALLOC_ZVAL(segmentObject);
}
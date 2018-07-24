//
// Created by tangzy on 18年7月23日.
//
#include "curl/curl.h"
#include "curl/easy.h"

#include "molten_log.h"
#include "molten_http_util.h"


http_result *result = NULL;

/* deal with the http response */
static size_t process_data(void *data, size_t size, size_t nmemb, char *content) {
    SLOG(SLOG_INFO, "[get][http] http response data:%s", data);
    //PTG(application_server) = (char*)data;
    result->response = (char*)data;
    return 0;
}

static CURLcode get_reqeust(char *get_uri)
{
    SLOG(SLOG_INFO, "[get][http] http data sender, get_uri:%s", get_uri);
    if (get_uri != NULL && strlen(get_uri) > 5) {
        CURL *curl = curl_easy_init();
        if (curl) {
            CURLcode res;

            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &process_data);

            curl_easy_setopt(curl, CURLOPT_URL, get_uri);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 10000L);

            //curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);

            res = curl_easy_perform(curl);
            //curl_easy_perform(curl);
            SLOG(SLOG_INFO, "[get][http] curl response code:%d", res);
            curl_easy_cleanup(curl);
            //avoid unused warning
            return res;
        }
        SLOG(SLOG_INFO, "[get][http] init curl error");
        return CURLE_FAILED_INIT;
    }
}

static char* request(char *uri, char *post_data) {
    CURLcode ret_code;
    if (post_data == NULL) {
        ret_code = get_reqeust(uri);
    } else {
        ret_code = post_reqeust(uri, post_data);
    }
    if (ret_code == CURLE_OK) {
        return result->response;
    }
    return NULL;
}

/* {{{ post request , current use curl not php_stream */
static CURLcode post_reqeust(char *uri, char *post_data)
{
    SLOG(SLOG_INFO, "[post][http] http data sender, post_uri:%s", uri);
    if (uri != NULL && strlen(uri) > 5) {
        CURL *curl = curl_easy_init();
        if (curl) {
            CURLcode res;
            struct curl_slist *list = NULL;

            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &process_data);

            list = curl_slist_append(list, "Content-Type: application/json");
            curl_easy_setopt(curl, CURLOPT_URL, uri);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 10000L);
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
            //curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);

            res = curl_easy_perform(curl);
            //curl_easy_perform(curl);
            SLOG(SLOG_INFO, "[post][http] curl response code:%d", res);
            curl_easy_cleanup(curl);
            curl_slist_free_all(list);
            //avoid unused warning
            return res;
        }
        SLOG(SLOG_INFO, "[post][http] init curl error");
        return CURLE_FAILED_INIT;
    }
}


/* {{{ trans log by http , current use curl not php_stream */
/*
static void get_reqeust(char *get_uri)
{
    SLOG(SLOG_INFO, "[get][http] http data sender, get_uri:%s", get_uri);
    if (get_uri != NULL && strlen(get_uri) > 5) {
        CURL *curl = curl_easy_init();
        if (curl) {
            CURLcode res;

            curl_easy_setopt(curl, CURLOPT_URL, get_uri);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 10000L);
            //curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);

            res = curl_easy_perform(curl);
            //curl_easy_perform(curl);
            SLOG(SLOG_INFO, " curl request code:%d", res);
            if (res != CURLE_OK) {
                switch(res) {
                    case CURLE_UNSUPPORTED_PROTOCOL:
                        fprintf(stderr,"不支持的协议,由URL的头部指定\n");
                    case CURLE_COULDNT_CONNECT:
                        fprintf(stderr,"不能连接到remote主机或者代理\n");
                    case CURLE_HTTP_RETURNED_ERROR:
                        fprintf(stderr,"http返回错误\n");
                    case CURLE_READ_ERROR:
                        fprintf(stderr,"读本地文件错误\n");
                    default:
                        fprintf(stderr,"返回值:%d\n",res);
                }
            }

            curl_easy_cleanup(curl);
            //avoid unused warning
            (void)res;
        } else {
            SLOG(SLOG_INFO, "[get][http] init curl error");
        }
    }
}*/


/* {{{ trans log by http , current use curl not php_stream */
/*
static void post_reqeust(char *post_uri, char *post_data)
{
    SLOG(SLOG_INFO, "[post][http] http data sender, post_uri:%s", post_uri);
    if (post_uri != NULL && strlen(post_uri) > 5) {
        CURL *curl = curl_easy_init();
        if (curl) {
            CURLcode res;
            struct curl_slist *list = NULL;

            list = curl_slist_append(list, "Content-Type: application/json");
            curl_easy_setopt(curl, CURLOPT_URL, post_uri);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 10000L);
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
            //curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);

            res = curl_easy_perform(curl);
            //curl_easy_perform(curl);
            SLOG(SLOG_INFO, " curl request code:%d", res);
            if (res != CURLE_OK) {
                switch(res) {
                    case CURLE_UNSUPPORTED_PROTOCOL:
                        fprintf(stderr,"不支持的协议,由URL的头部指定\n");
                    case CURLE_COULDNT_CONNECT:
                        fprintf(stderr,"不能连接到remote主机或者代理\n");
                    case CURLE_HTTP_RETURNED_ERROR:
                        fprintf(stderr,"http返回错误\n");
                    case CURLE_READ_ERROR:
                        fprintf(stderr,"读本地文件错误\n");
                    default:
                        fprintf(stderr,"返回值:%d\n",res);
                }
            }
            curl_easy_cleanup(curl);
            curl_slist_free_all(list);
            //avoid unused warning
            (void)res;
        } else {
            SLOG(SLOG_INFO, "[post][http] init curl error");
        }
    }
}*/

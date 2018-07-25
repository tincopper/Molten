//
// Created by tangzy on 18年7月23日.
//
#include "curl/curl.h"
#include "curl/easy.h"

#include "molten_log.h"
#include "common/molten_slog.h"
#include "molten_http_util.h"

//char* response = NULL;

/* deal with the http response */
size_t process_data(void *data, size_t size, size_t nmemb, char *content) {
    SLOG(SLOG_INFO, "[http] http response data:%s", data);
    long sizes = size * nmemb;
    strncpy(content, (char*)data, sizes);
    //response = (char*)data;
    return sizes;
}

CURLcode get_request(char *uri, char *response) {
    SLOG(SLOG_INFO, "[get][http] http data sender, uri:%s", uri);
    if (uri != NULL && strlen(uri) > 5) {
        CURL *curl = curl_easy_init();
        CURLcode res = CURLE_OK;
        if (curl) {

            // set params
            curl_easy_setopt(curl, CURLOPT_URL, uri); // url
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0); // if want to use https
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0); // set peer and host verify false
            //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
            //curl_easy_setopt(curl, CURLOPT_READFUNCTION, NULL);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &process_data);
            //curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
            curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
            //curl_easy_setopt(curl, CURLOPT_HEADER, 1);
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30);

            res = curl_easy_perform(curl);
            SLOG(SLOG_INFO, "[get][http] curl response code:%d", res);
        } else {
            SLOG(SLOG_INFO, "[get][http] init curl error");
            res = CURLE_FAILED_INIT;
        }

        curl_easy_cleanup(curl);
        return res;
    }
}

/* {{{ post request , current use curl not php_stream */
CURLcode post_request(char *uri, char *post_data, char *response) {
    SLOG(SLOG_INFO, "[post][http] http data sender, uri:[%s], post_data:[%s]", uri, post_data);
    if (uri != NULL && strlen(uri) > 5) {
        CURL *curl = curl_easy_init();
        CURLcode res = CURLE_OK;
        if (curl) {

            // set params
            curl_easy_setopt(curl, CURLOPT_POST, 1); // post req
            curl_easy_setopt(curl, CURLOPT_URL, uri); // url
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data); // params
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0); // if want to use https
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0); // set peer and host verify false
            //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
            //curl_easy_setopt(curl, CURLOPT_READFUNCTION, NULL);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &process_data);
            //curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
            curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
            //curl_easy_setopt(curl, CURLOPT_HEADER, 1);
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30);

            res = curl_easy_perform(curl);
            SLOG(SLOG_INFO, "[post][http] curl response code:%d", res);

        } else {
            SLOG(SLOG_INFO, "[post][http] init curl error");
            res = CURLE_FAILED_INIT;
        }

        curl_easy_cleanup(curl);
        return res;
    }
}

/*char* request(char *uri, char *post_data) {
    CURLcode ret_code = CURLE_OK;
    response = NULL;

    if (post_data == NULL) {
        ret_code = get_request(uri);
    } else {
        ret_code = post_request(uri, post_data);
    }

    if (ret_code == CURLE_OK) {
        return response;
    }
    return NULL;
}*/

CURL* init() {
    return curl_easy_init();
}

void release(CURL* curl) {
    curl_easy_cleanup(curl);
}

void globale_init() {
    curl_global_init(CURL_GLOBAL_ALL);
}

void globale_release() {
    curl_global_cleanup();
}
//
// Created by tangzy on 18年7月23日.
//
#include "curl/curl.h"
#include "curl/easy.h"

#include "molten_log.h"
#include "common/molten_slog.h"
#include "molten_http_util.h"

/* deal with the http response */
size_t process_data(void *data, size_t size, size_t nmemb, char *content) {
    long sizes = size * nmemb;
    strncpy(content, (char*)data, sizes);
    return sizes;
}

CURLcode get_request(char *uri, char *response) {
    if (uri != NULL && strlen(uri) > 5) {
        CURL *curl = curl_easy_init();
        CURLcode res = CURLE_OK;
        if (curl) {

            // set params
            curl_easy_setopt(curl, CURLOPT_URL, uri); // url
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0); // if want to use https
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0); // set peer and host verify false
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &process_data);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
            curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30);

            res = curl_easy_perform(curl);
        } else {
            SLOG(SLOG_ERROR, "[get][http] init curl error");
            res = CURLE_FAILED_INIT;
        }

        curl_easy_cleanup(curl);
        return res;
    }
}

/* {{{ post request , current use curl not php_stream */
CURLcode post_request(char *uri, char *post_data, char *response) {
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
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &process_data);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
            curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30);

            res = curl_easy_perform(curl);
        } else {
            SLOG(SLOG_ERROR, "[post][http] init curl error");
            res = CURLE_FAILED_INIT;
        }

        curl_easy_cleanup(curl);
        return res;
    }
}
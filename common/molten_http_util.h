//
// Created by tangzy on 18年7月23日.
//

#ifndef MOLTEN_MOLTEN_HTTP_UTIL_H
#define MOLTEN_MOLTEN_HTTP_UTIL_H

typedef struct {
    uint8_t ret_code;
    char*   response;
} http_result;

extern http_result *result; // 注意，这里不能再初始化
//static CURLcode post_reqeust(char *post_uri, char *post_data);
//static CURLcode get_request(char *get_uri);

static char* request(char *get_uri, char *post_data);

#endif
//
// Created by tangzy on 18年7月23日.
//

#include <curl/curl.h>
#include "molten_http_util.h"

int main() {
    char * server_url = "http://10.40.6.114:10800/agent/jetty";
    get_reqeust(server_url);
    return 0;
}
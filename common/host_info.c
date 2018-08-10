//
// Created by tangzy on 18-7-26.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <ifaddrs.h>
#include "host_info.h"


LONG current_system_time_millis() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    /**
     * int ：表示数据范围在-32768到32767之间
     * long int : 表示的范围在-2147483648到2147483647之间
     * long long : 表示的范围在-9223372036854775808 到 9223372036854775807之间
     *
     * 毫秒有13位超过了long int范围，所需要转为long long类型
     */
    LONG t_sec = (LONG)tv.tv_sec;
    LONG t_usec = (LONG)tv.tv_usec;
    return (t_sec * 1000000LL + t_usec) / 1000LL;
}

int current_thread_pid() {
    return getpid();
}

int current_parent_thread_pid() {
    return getppid();
}

char *current_host_name() {
    static char hostname[64]; //这里要定义为static，因为此定义的为局部变量，执行完会回收

    if (gethostname(hostname, sizeof(hostname)) < 0) {
        perror("gethostname");
        exit(1);
    }
    return hostname;
}

long current_host_id() {
    long hostid;
    if ((hostid = gethostid()) < 0) {
        perror("gethostid");
        exit(1);
    }
    return hostid;
}

struct utsname current_host_info() {
    struct utsname buf;
    if (uname(&buf)) {
        perror("uname");
        exit(1);
    }
    return buf;
}

char *current_os_name() {
    static char result[32];
    struct utsname utsname = current_host_info();
    strcpy(result, utsname.sysname);
    return result;
}

char **current_ipv4() {
    struct ifaddrs *addrs;
    void *tmpAddrPtr = NULL;
    static char *ipv4s[6] = {};
    //char result[]
    if (getifaddrs(&addrs)) {
        perror("getifaddrs");
        exit(1);
    }
    int i = 0;
    while (addrs != NULL && i < 6) {
        if (addrs->ifa_addr->sa_family == AF_INET &&  strcmp(addrs->ifa_name, "lo") != 0) { // check it is IP4 and exclusion loop address
            // is a valid IP4 Address
            tmpAddrPtr = &((struct sockaddr_in *)addrs->ifa_addr)->sin_addr;
            char addressBuffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
            //printf("%s IPv4 Address %s\n", addrs->ifa_name, addressBuffer);
            ipv4s[i++] = addressBuffer;
        }
        addrs = addrs->ifa_next;
    }
    return ipv4s;
}

char *current_ipv6() {
    struct ifaddrs *addrs;
    void *tmpAddrPtr = NULL;
    if (getifaddrs(&addrs)) {
        perror("getifaddrs");
        exit(1);
    }
    while (addrs != NULL) {
        if (addrs->ifa_addr->sa_family == AF_INET6) { // check it is IP6
            // is a valid IP6 Address
            tmpAddrPtr = &((struct sockaddr_in *)addrs->ifa_addr)->sin_addr;
            char addressBuffer[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, tmpAddrPtr, addressBuffer, INET6_ADDRSTRLEN);
            printf("%s IPv6 Address %s\n", addrs->ifa_name, addressBuffer);
        }
        addrs = addrs->ifa_next;
    }
}
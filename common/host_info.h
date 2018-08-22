//
// Created by tangzy on 18-7-26.
//

#ifndef HOST_INFO_H
#define HOST_INFO_H

#define LONG long long
#define INT long int

/**
 * 获取当期系统时间毫秒数
 * @return 时间毫秒数
 */
LONG current_system_time_millis();

/**
 * 获取当前运行线程的pid
 * @return pid
 */
int current_thread_pid();

/**
 * 获取当期运行线程的父线程pid
 * @return parent pid
 */
int current_parent_thread_pid();

/**
 * 获取当期系统的主机名
 * @return hostname
 */
char *current_host_name();

/**
 * 获取当期系统的主机id
 * @return hostid
 */
long current_host_id();

/**
 * 返回系统当期的常规信息
 * @return host_info
 */
struct utsname current_host_info();

/**
 * 获取当前系统的OS名称
 * @return
 */
char *current_os_name();

/**
 * 获取当前系统的ipv4地址
 * @return string[]
 */
char **current_ipv4();

/**
 * 获取当前系统的ipv6地址
 * @return
 */
char *current_ipv6();

/**
 * 获取当前系统的mac地址
 * @return
 */
char *current_host_mac();

#endif
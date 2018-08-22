//
// Created by root on 18-8-13.
//

/**
 * The load balancing polling obtains the service information of the skywalking collection end
 * to avoid single point failure of reporting data and reduce the load of a node.
 */

#ifndef LOAD_BALANCE_H
#define LOAD_BALANCE_H

#define SERVER_NAME_LEN    32
#define DEFAULT_WEIGHT     50

typedef struct
{
    int weight;
    int cur_weight;
    char name[SERVER_NAME_LEN];
} server;

server *initServers(char **names, int *weights, int size);

server *initDefaultServers(char **names, int size);

int getNextServerIndex(server *ss, int size);

#endif
//
// Created by tomgs on 18-8-13.
//

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "load_balance.h"

/**
 * init servers
 * @param names server names
 * @param weights server names's weight
 * @param size names size
 * @return server object
 */
server *initServers(char **names, int *weights, int size)
{
    int i = 0;
    server *ss = calloc(size+1, sizeof(server));

    for (i = 0; i < size; i++) {
        ss[i].weight = weights[i];
        memcpy(ss[i].name, names[i], SERVER_NAME_LEN);
        ss[i].cur_weight = 0;
    }
    return ss;
}

/**
 * init defualt servers, the weight is 50%
 * @param names  server names
 * @param size   server num
 * @return
 */
server *initDefaultServers(char **names, int size)
{
    int i = 0;
    server *ss = calloc(size+1, sizeof(server));

    for (i = 0; i < size; i++) {
        ss[i].weight = DEFAULT_WEIGHT;
        memcpy(ss[i].name, names[i], SERVER_NAME_LEN);
        ss[i].cur_weight = 0;
    }
    return ss;
}

/**
 * get next server index
 * @param ss server names
 * @param size names size
 * @return server index
 */
int getNextServerIndex(server *ss, int size)
{
    int i ;
    int index = -1;
    int total = 0;

    for (i = 0; i < size; i++) {
        ss[i].cur_weight += ss[i].weight;
        total += ss[i].weight;

        if (index == -1 || ss[index].cur_weight < ss[i].cur_weight) {
            index = i;
        }
    }

    ss[index].cur_weight -= total;
    return index;
}

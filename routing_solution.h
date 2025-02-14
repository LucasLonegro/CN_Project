#ifndef RTNG_H
#define RTNG_H
#include "network.h"

typedef struct connection_request
{
    uint64_t from_node_id;
    uint64_t to_node_id;
    uint64_t load;
} connection_request;

typedef struct routing_assignment
{
    path_t *path;
    uint64_t load;
} routing_assignment;

void generate_routing(const network_t *network, connection_request *requests, uint64_t requests_dim, routing_assignment *assignments_ret);

#endif

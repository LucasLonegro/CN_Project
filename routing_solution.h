#ifndef RTNG_H
#define RTNG_H
#include "network.h"
#include "problem.h"
#include "utils.h"

typedef struct connection_request
{
    uint64_t from_node_id;
    uint64_t to_node_id;
    uint64_t load;
} connection_request;

typedef struct assignment_t
{
    path_t *path;
    uint64_t load;
    uint64_t start_slot;
    uint64_t end_slot;
    uint64_t format;
    int is_split;
    struct assignment_t *split;
} assignment_t;

void generate_routing(const network_t *network, connection_request *requests, uint64_t requests_dim, const modulation_format *formats, uint64_t formats_dim, assignment_t *assignments_ret, dynamic_char_array *link_slot_usages_ret);

#endif

#ifndef RTNG_H
#define RTNG_H
#include "network.h"
#include "problem.h"
#include "utils.h"

// Increase to give greater weight to number of links in path vs load when ranking paths in least_used_path algorithm
#define LENGTH_LOAD_PONDERATION 1000.0

typedef enum slot_status
{
    UNUSED = 0,
    USED,
    GUARD_BAND
} slot_status;

typedef enum routing_algorithms
{
    FIXED_SHORTEST_PATH,
    LEAST_USED_PATH
} routing_algorithms;

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

void generate_routing(const network_t *network, connection_request *requests, uint64_t requests_dim, const modulation_format *formats, uint64_t formats_dim, assignment_t *assignments_ret, dynamic_char_array *link_slot_usages_ret, routing_algorithms algorithm);

#endif

#ifndef RTNG_H
#define RTNG_H
#include "network.h"
#include "problem.h"
#include "utils.h"

// Increase to give greater weight to number of links in path vs load when ranking paths in least_used_path algorithm
#define LENGTH_LOAD_PONDERATION 1000.0
#define OVERLAP_SHARED_PROTECTION_SLOTS 1

typedef enum slot_status
{
    UNUSED = 0,
    USED,
    PROTECTION_USED,
    GUARD_BAND,
    PROTECTION_GUARD_BAND
} slot_status;

typedef enum routing_algorithms
{
    FIXED_SHORTEST_PATH,
    LEAST_USED_PATH,
    LEAST_USED_PATH_JOINT
} routing_algorithms;

typedef enum slot_assignment_algorithms
{
    LEAST_USED_SLOT,
    FIRST_FIT_SLOT,
    NON_CONTINUOUS = 1 << 3,
    NON_CONTIGUOUS = 1 << 4
} slot_assignment_algorithms;

typedef enum protection_type
{
    NO_PROTECTION,
    DEDICATED_PROTECTION,
    SHARED_PROTECTION
} protection_type;

typedef enum modulation_format_assignment_algorithms
{
    DEFAULT_MODULATION,
} modulation_format_assignment_algorithms;

typedef struct connection_request
{
    uint64_t from_node_id;
    uint64_t to_node_id;
    uint64_t load;
} connection_request;

typedef struct assignment_t
{
    const path_t *path;
    uint64_t load;
    uint64_t start_slot;
    uint64_t end_slot;
    uint64_t format;
    int is_split;
    struct assignment_t *split;
} assignment_t;

/**
 * @brief generates a routing solution given a set of RMSA algorithms
 *
 * @param network
 * @param requests
 * @param requests_dim
 * @param formats
 * @param formats_dim
 * @param assignments_ret
 * @param protections_ret
 * @param protection
 * @param link_slot_usages_ret
 * @param routing_algorithm
 * @param slot_assigner_algorithm
 * @param format_assigner
 * @return path_t** a NULL-terminated array of paths that must be freed, as well as the array. Can be NULL if empty.
 */
path_t **generate_routing(const network_t *network, connection_request *requests, uint64_t requests_dim, const modulation_format *formats, uint64_t formats_dim, assignment_t *assignments_ret, assignment_t *protections_ret, protection_type protection, dynamic_char_array *link_slot_usages_ret, routing_algorithms routing_algorithm, slot_assignment_algorithms slot_assigner_algorithm, modulation_format_assignment_algorithms format_assigner);

#endif

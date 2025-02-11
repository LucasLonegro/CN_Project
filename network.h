#ifndef NTWK_H
#define NTWK_H
#include <stdint.h>

typedef struct path_t
{
    uint64_t *nodes;
    __ssize_t length;
    __ssize_t distance;
} path_t;

typedef struct network_t
{
    uint64_t node_count;
    __ssize_t *adjacency_matrix;
    path_t ***shortest_paths;
    path_t ***shortest_unweighted_paths;
} network_t;


network_t *new_network(uint64_t node_count);
void free_network(network_t *network);

int set_link_weight(network_t *network, uint64_t nodeid1, uint64_t nodeid2, __ssize_t weight);

__ssize_t get_link_weight(const network_t *network, uint64_t nodeid1, uint64_t nodeid2);

path_t *const*weighted_distances(const network_t *network, uint64_t nodeid);
path_t *const*unweighted_distances(const network_t *network, uint64_t nodeid);

#endif

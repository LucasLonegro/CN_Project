#ifndef NTWK_H
#define NTWK_H
#include <stdint.h>
#include <stdio.h>
#ifdef _WIN32
#include <stddef.h>
typedef signed long __ssize_t;  
#endif

typedef struct path_t
{
    uint64_t *nodes;
    __ssize_t length;
    __ssize_t distance;
} path_t;

typedef struct k_paths
{
    uint64_t k;
    path_t **paths;
} k_paths;

typedef struct network_t
{
    uint64_t node_count;
    __ssize_t *adjacency_matrix;
    path_t ***shortest_paths;
    path_t ***shortest_unweighted_paths;
    k_paths *k_shortest_weighted_paths;
} network_t;

typedef int (*link_validator)(const network_t *network, uint64_t from, uint64_t to, const void *data);
typedef struct link_validator_list_entry
{
    const void *data;
    link_validator validator;
} link_validator_list_entry;

typedef struct link_validator_list
{
    link_validator_list_entry *entries;
    uint64_t len;
} link_validator_list;
int noop_validator(const network_t *network, uint64_t from, uint64_t to, const void *data);
int validator_joiner(const network_t *network, uint64_t from, uint64_t to, const void *data);

network_t *new_network(uint64_t node_count);
void free_network(network_t *network);

int set_link_weight(network_t *network, uint64_t from, uint64_t to, __ssize_t weight);

__ssize_t get_link_weight(const network_t *network, uint64_t from, uint64_t to);

path_t *const *weighted_distances(const network_t *network, uint64_t from);
path_t *const *unweighted_distances(const network_t *network, uint64_t from);
path_t *const *k_shortest_paths(const network_t *network, uint64_t from, uint64_t to, uint64_t k);

path_t **modified_k_shortest_paths(const network_t *network, uint64_t from, uint64_t to, uint64_t k, link_validator validator, const void *data);
path_t **modified_weighted_distances(const network_t *network, uint64_t from, link_validator validator, const void *data);

void free_distances(const network_t *network, path_t **distances);

void print_path(const path_t *p, FILE *file);

#endif

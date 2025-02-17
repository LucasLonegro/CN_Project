#include "network.h"
#include <stdlib.h>

#define IS_BIT_SET(x, i) (((x)[(i) >> 3] & (1 << ((i) & 7))) != 0)
#define SET_BIT(x, i) (x)[(i) >> 3] |= (1 << ((i) & 7))
#define CLEAR_BIT(x, i) (x)[(i) >> 3] &= (1 << ((i) & 7)) ^ 0xFF

#define LINK_WEIGHT(n, i1, i2) ((n)->adjacency_matrix[(i1) * (n)->node_count + (i2)])
#define SET_LINK_WEIGHT(n, i1, i2, w) ((n)->adjacency_matrix[(i1) * (n)->node_count + (i2)] = (w))

network_t *new_network(uint64_t node_count)
{
    network_t *network = malloc(sizeof(network_t) * 1);
    if (network == NULL)
        return NULL;
    network->node_count = node_count;
    network->adjacency_matrix = malloc(sizeof(__ssize_t) * node_count * node_count);
    network->shortest_paths = calloc(node_count, sizeof(path_t *));
    network->shortest_unweighted_paths = calloc(node_count, sizeof(path_t *));
    for (uint64_t i = 0; i < node_count * node_count; i++)
        network->adjacency_matrix[i] = -1;
    return network;
}

void free_network(network_t *network)
{
    for (uint64_t i = 0; i < network->node_count; i++)
    {
        if (network->shortest_paths[i] != NULL)
        {
            for (uint64_t j = 0; j < network->node_count; j++)
            {
                free(network->shortest_paths[i][j]->nodes);
                free(network->shortest_paths[i][j]);
            }
            free(network->shortest_paths[i]);
        }
        if (network->shortest_unweighted_paths[i] != NULL)
        {
            for (uint64_t j = 0; j < network->node_count; j++)
            {
                free(network->shortest_unweighted_paths[i][j]->nodes);
                free(network->shortest_unweighted_paths[i][j]);
            }
            free(network->shortest_unweighted_paths[i]);
        }
    }
    free(network->shortest_paths);
    free(network->shortest_unweighted_paths);
    free(network->adjacency_matrix);
    free(network);
}

int set_link_weight(network_t *network, uint64_t from, uint64_t to, __ssize_t weight)
{
    if (from > network->node_count || to > network->node_count)
        return 0;
    if (LINK_WEIGHT(network, from, to) != -1)
    {
        return 0;
    }
    network->adjacency_matrix[from * network->node_count + to] = weight;
    return 1;
}

__ssize_t get_link_weight(const network_t *network, uint64_t from, uint64_t to)
{
    if (from > network->node_count || to > network->node_count)
        return -1;
    return LINK_WEIGHT(network, from, to);
}

__ssize_t get_link_distance(const network_t *network, uint64_t from, uint64_t to)
{
    if (from > network->node_count || to > network->node_count)
        return -1;
    return LINK_WEIGHT(network, from, to) == -1 ? -1 : 1;
}

typedef __ssize_t read_link_weight(const network_t *, uint64_t, uint64_t);

__ssize_t minDistance(path_t **distances, char *visited, uint64_t node_count)
{
    // Initialize min value
    __ssize_t min = -1, min_index = -1;

    for (uint64_t v = 0; v < node_count; v++)
    {
        if (!IS_BIT_SET(visited, v) && (distances[v]->distance != -1 && (distances[v]->distance <= min || min == -1)))
        {
            min = distances[v]->distance;
            min_index = v;
        }
    }

    return min_index;
}

void copy_array(uint64_t *target, const uint64_t *source, uint64_t length)
{
    for (uint64_t i = 0; i < length; i++)
    {
        target[i] = source[i];
    }
}

int noop_validator(const network_t *network, uint64_t from, uint64_t to, const void *data)
{
    return 1;
}

path_t **modified_dijkstra(const network_t *network, uint64_t from, read_link_weight read_function, link_validator validator, void *data);
path_t **dijkstra(const network_t *network, uint64_t from, read_link_weight read_function)
{
    return modified_dijkstra(network, from, read_function, noop_validator, NULL);
}

path_t *const *weighted_distances(const network_t *network, uint64_t from)
{
    if (network->shortest_paths[from] != NULL)
        return network->shortest_paths[from];
    path_t **distances = dijkstra(network, from, get_link_weight);
    network->shortest_paths[from] = distances;
    return distances;
}

path_t *const *unweighted_distances(const network_t *network, uint64_t from)
{
    if (network->shortest_unweighted_paths[from] != NULL)
        return network->shortest_unweighted_paths[from];
    path_t **distances = dijkstra(network, from, get_link_distance);
    network->shortest_unweighted_paths[from] = distances;
    return distances;
}

path_t **modified_weighted_distances(const network_t *network, uint64_t from, link_validator validator, void *data)
{
    return modified_dijkstra(network, from, get_link_weight, validator, data);
}

void free_distances(const network_t *network, path_t **distances)
{
    for (uint64_t i = 0; i < network->node_count; i++)
    {
        free(distances[i]->nodes);
    }
    free(distances);
}

path_t **modified_dijkstra(const network_t *network, uint64_t from, read_link_weight read_function, link_validator validator, void *data)
{
    if (from > network->node_count)
        return NULL;
    char *visited = calloc((network->node_count >> 3) + 1, sizeof(char));

    path_t **distances = calloc(network->node_count, sizeof(path_t *));
    for (__ssize_t i = 0; i < network->node_count; i++)
    {
        distances[i] = malloc(sizeof(path_t));
        distances[i]->nodes = malloc(sizeof(uint64_t) * network->node_count);
        distances[i]->nodes[0] = from;
        distances[i]->distance = -1;
        distances[i]->length = -1;
    }

    distances[from]->distance = 0;
    distances[from]->length = 0;
    for (uint64_t count = 0; count < network->node_count - 1; count++)
    {
        int u = minDistance(distances, visited, network->node_count);
        if (u == -1)
            continue;
        SET_BIT(visited, u);
        for (uint64_t v = 0; v < network->node_count; v++)
        {
            __ssize_t link_distance = read_function(network, u, v);
            __ssize_t current_distance = distances[u]->distance;
            if (!IS_BIT_SET(visited, v) && link_distance != -1 && current_distance != -1 && (current_distance + link_distance < distances[v]->distance || distances[v]->distance == -1) && validator(network, u, v, data))
            {
                distances[v]->distance = distances[u]->distance + link_distance;
                copy_array(distances[v]->nodes, distances[u]->nodes, distances[u]->length + 1);
                distances[v]->length = distances[u]->length + 1;
                distances[v]->nodes[distances[v]->length] = v;
            }
        }
    }

    free(visited);
    return distances;
}

#include "network.h"
#include <stdlib.h>
#include <string.h>

#define IS_BIT_SET(x, i) (((x)[(i) >> 3] & (1 << ((i) & 7))) != 0)
#define SET_BIT(x, i) (x)[(i) >> 3] |= (1 << ((i) & 7))
#define CLEAR_BIT(x, i) (x)[(i) >> 3] &= (1 << ((i) & 7)) ^ 0xFF

#define LINK_WEIGHT(n, i1, i2) ((n)->adjacency_matrix[(i1) * (n)->node_count + (i2)])
#define SET_LINK_WEIGHT(n, i1, i2, w) ((n)->adjacency_matrix[(i1) * (n)->node_count + (i2)] = (w))

int noop_validator(const network_t *network, uint64_t from, uint64_t to, const void *data);
void print_path(const path_t *p, FILE *file)
{
    fprintf(file, "\t");
    fprintf(file, "Length: %ld\t", p->length);
    fprintf(file, "Distance: %ld\t", p->distance);
    for (uint64_t j = 0; j <= p->length; j++)
    {
        if (p->length == -1 || p->distance == -1)
            break;
        fprintf(file, "%ld", p->nodes[j]);
        if (j != p->length)
            fprintf(file, "->");
    }
    fprintf(file, "\n");
}

network_t *new_network(uint64_t node_count)
{
    network_t *network = malloc(sizeof(network_t) * 1);
    if (network == NULL)
        return NULL;
    network->node_count = node_count;
    network->adjacency_matrix = malloc(sizeof(__ssize_t) * node_count * node_count);
    network->shortest_paths = calloc(node_count, sizeof(path_t *));
    network->shortest_unweighted_paths = calloc(node_count, sizeof(path_t *));
    network->k_shortest_weighted_paths = calloc(node_count * node_count, sizeof(k_paths));
    for (uint64_t i = 0; i < node_count * node_count; i++)
        network->adjacency_matrix[i] = -1;
    return network;
}

// only path element must be deallocated
path_t *subpath(const network_t *network, path_t *path, uint64_t end_node_index_inclusive)
{
    path_t *ans = calloc(1, sizeof(path_t));
    uint64_t distance = 0;
    for (uint64_t i = 0; i < end_node_index_inclusive && i < path->length; i++)
    {
        distance += get_link_weight(network, path->nodes[i], path->nodes[i + 1]);
    }
    ans->distance = distance;
    ans->length = end_node_index_inclusive;
    ans->nodes = path->nodes;
    return ans;
}

// both path element and its nodes must be deallocated
path_t *merge_paths(const path_t *p1, const path_t *p2)
{
    path_t *ans = calloc(1, sizeof(path_t));
    uint64_t len1 = p1->length == -1 ? 0 : p1->length, len2 = p2->length == -1 ? 0 : p2->length;

    ans->nodes = malloc((len1 + len2 + 1) * sizeof(uint64_t));
    ans->length = len1 + len2;
    ans->distance = p1->distance + p2->distance;
    for (uint64_t i = 0; i < len1; i++)
    {
        ans->nodes[i] = p1->nodes[i];
    }
    for (uint64_t i = len1; i <= len1 + len2; i++)
    {
        ans->nodes[i] = p2->nodes[i - len1];
    }
    return ans;
}

int are_equal_paths(const path_t *p1, const path_t *p2)
{
    if (p1->distance != p2->distance || p1->length != p2->length)
        return 0;
    for (uint64_t i = 0; i <= p1->length; i++)
    {
        if (p1->nodes[i] != p2->nodes[i])
            return 0;
    }
    return 1;
}

typedef struct removed_elements
{
    path_t *path;
    char *removed_links;
    char *removed_nodes;
} removed_elements;

int is_not_using_removed_elements_and_does_not_revisit_nodes(const network_t *network, uint64_t from, uint64_t to, const void *data)
{
    removed_elements *_data = (removed_elements *)(data);
    if (_data->path != NULL)
        for (uint64_t i = 0; i <= _data->path->length; i++)
            if (to == _data->path->nodes[i])
                return 0;
    return !IS_BIT_SET(_data->removed_nodes, from) && !IS_BIT_SET(_data->removed_nodes, to) && !IS_BIT_SET(_data->removed_links, from * network->node_count + to);
}

uint64_t contains_path(path_t **array, uint64_t array_dim, const path_t *path)
{
    for (uint64_t i = 0; i < array_dim; i++)
        if (are_equal_paths(array[i], path))
            return 1;
    return 0;
}

void sort_by_distance(path_t **array, uint64_t array_dim, int ascending)
{
    uint64_t sorted = 0;
    while (!sorted)
    {
        sorted = 1;
        for (uint64_t i = 0; i < array_dim - 1; i++)
        {
            if ((ascending && array[i]->distance > array[i + 1]->distance) || (!ascending && array[i]->distance < array[i + 1]->distance))
            {
                sorted = 0;
                path_t *aux = array[i];
                array[i] = array[i + 1];
                array[i + 1] = aux;
            }
        }
    }
}

path_t **modified_yens_algorithm(const network_t *network, uint64_t from, uint64_t to, uint64_t n, link_validator validator, const void *validator_data);
// abandon all hope ye who enter here
path_t **yens_algorithm(const network_t *network, uint64_t from, uint64_t to, uint64_t n)
{
    return modified_yens_algorithm(network, from, to, n, noop_validator, NULL);
}

int validator_joiner(const network_t *network, uint64_t from, uint64_t to, const void *data)
{
    link_validator_list *_data = (link_validator_list *)data;
    for (uint64_t i = 0; i < _data->len; i++)
    {
        if (_data->entries[i].validator != NULL && !(_data->entries[i].validator(network, from, to, _data->entries[i].data)))
            return 0;
    }
    return 1;
}

// abandon all hope ye who enter here
path_t **modified_yens_algorithm(const network_t *network, uint64_t from, uint64_t to, uint64_t n, link_validator validator, const void *validator_data)
{
    path_t **A = calloc(n, sizeof(path_t *));
    uint64_t A_size = 0;
    path_t **B = calloc((n + 1) * network->node_count, sizeof(path_t));
    uint64_t B_size = 0;

    uint64_t count = 0;

    char *edges_are_removed = calloc(((network->node_count * network->node_count) >> 3) + 1, sizeof(char));
    char *nodes_are_removed = calloc((network->node_count >> 3) + 1, sizeof(char));
    removed_elements data = {.removed_links = edges_are_removed, .removed_nodes = nodes_are_removed};

    link_validator_list validator_join;

    link_validator_list_entry entries[] = {{.validator = is_not_using_removed_elements_and_does_not_revisit_nodes, .data = (void *)&data}, {.validator = validator, .data = validator_data}};
    validator_join.len = 2;
    validator_join.entries = entries;

    path_t **aux = modified_weighted_distances(network, from, validator, validator_data);
    for (uint64_t i = 0; i < network->node_count; i++)
        if (i != to)
        {
            free(aux[i]->nodes);
            free(aux[i]);
        }
    A[0] = aux[to];
    free(aux);
    if (A[0]->distance == -1)
    {
        free(B);
        free(nodes_are_removed);
        free(edges_are_removed);
        return A;
    }
    A_size = 1;

    for (uint64_t k = 0; k < n - 1; k++)
    {
        for (uint64_t i = 0; i < A[k]->length; i++)
        {
            uint64_t spur_node = A[k]->nodes[i];
            path_t *root_path = subpath(network, A[k], i);

            for (uint64_t path_index = 0; path_index < A_size; path_index++)
            {
                path_t *aux = subpath(network, A[path_index], i);
                if (are_equal_paths(root_path, aux))
                {
                    SET_BIT(edges_are_removed, A[path_index]->nodes[i] * network->node_count + A[path_index]->nodes[i + 1]);
                }
                free(aux);
            }
            for (uint64_t node_index = 1; node_index <= root_path->length; node_index++)
            {
                if (root_path->nodes[node_index] != spur_node)
                {
                    SET_BIT(nodes_are_removed, root_path->nodes[node_index]);
                }
            }
            path_t *subpath_to_spur_node = subpath(network, A[k], i);
            data.path = subpath_to_spur_node;
            path_t **spur_paths_aux = modified_weighted_distances(network, spur_node, validator_joiner, (void *)&validator_join);
            free(subpath_to_spur_node);
            for (uint64_t i = 0; i < network->node_count; i++)
                if (i != to)
                {
                    free(spur_paths_aux[i]->nodes);
                    free(spur_paths_aux[i]);
                }
            path_t *spur_path = spur_paths_aux[to];
            memset(edges_are_removed, 0, (((network->node_count * network->node_count) >> 3) + 1) * sizeof(char));
            memset(nodes_are_removed, 0, (((network->node_count) >> 3) + 1) * sizeof(char));

            if (spur_path->length == -1 || spur_path->distance == -1)
            {
                free(spur_path->nodes);
                free(spur_path);
                free(spur_paths_aux);
                free(root_path);
                continue;
            }

            path_t *total_path = merge_paths(root_path, spur_path);
            free(spur_path->nodes);
            free(spur_path);
            free(spur_paths_aux);
            free(root_path);

            if (!contains_path(B, B_size, total_path))
            {
                B[B_size++] = total_path;
                count++;
            }
            else
            {
                free(total_path->nodes);
                free(total_path);
            }
        }
        if (B_size == 0)
        {
            break;
        }

        sort_by_distance(B, B_size, 0);
        A[A_size++] = B[--B_size];
    }
    for (uint64_t i = 0; i < B_size; i++)
    {
        if (!contains_path(A, A_size, B[i]))
        {
            free(B[i]->nodes);
            free(B[i]);
        }
    }
    free(B);
    free(nodes_are_removed);
    free(edges_are_removed);
    return A;
}

path_t **modified_k_shortest_paths(const network_t *network, uint64_t from, uint64_t to, uint64_t k, link_validator validator, const void *data)
{
    if (validator == NULL)
        validator = noop_validator;
    return modified_yens_algorithm(network, from, to, k, validator, data);
}

// I shall dub you the memory fragmentator
path_t *const *k_shortest_paths(const network_t *network, uint64_t from, uint64_t to, uint64_t k)
{
    if (network->k_shortest_weighted_paths[from * network->node_count + to].k != 0)
    {
        if (network->k_shortest_weighted_paths[from * network->node_count + to].k >= k)
            return network->k_shortest_weighted_paths[from * network->node_count + to].paths;
        else
        {
            for (uint64_t i = 0; i < network->node_count; i++)
            {
                if (network->k_shortest_weighted_paths[from * network->node_count + to].paths[i] != NULL)
                {
                    free(network->k_shortest_weighted_paths[from * network->node_count + to].paths[i]->nodes);
                    free(network->k_shortest_weighted_paths[from * network->node_count + to].paths[i]);
                }
            }
            free(network->k_shortest_weighted_paths[from * network->node_count + to].paths);
        }
    }
    path_t **k_paths = yens_algorithm(network, from, to, k);
    network->k_shortest_weighted_paths[from * network->node_count + to].k = k;
    network->k_shortest_weighted_paths[from * network->node_count + to].paths = k_paths;
    return k_paths;
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
    for (uint64_t i = 0; i < network->node_count * network->node_count; i++)
    {
        if (network->k_shortest_weighted_paths[i].k != 0)
        {
            for (uint64_t j = 0; j < network->k_shortest_weighted_paths[i].k; j++)
            {
                if (network->k_shortest_weighted_paths[i].paths[j] != NULL)
                {
                    free(network->k_shortest_weighted_paths[i].paths[j]->nodes);
                    free(network->k_shortest_weighted_paths[i].paths[j]);
                }
            }
            free(network->k_shortest_weighted_paths[i].paths);
        }
    }
    free(network->k_shortest_weighted_paths);
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

path_t **modified_dijkstra(const network_t *network, uint64_t from, read_link_weight read_function, link_validator validator, const void *data);
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

path_t **modified_weighted_distances(const network_t *network, uint64_t from, link_validator validator, const void *data)
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

path_t **modified_dijkstra(const network_t *network, uint64_t from, read_link_weight read_function, link_validator validator, const void *data)
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

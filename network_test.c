#include <stdio.h>
#include "network.h"

#define V 9
int main(void)
{
    int graph[V][V] = {{0, 4, 0, 0, 0, 0, 0, 8, 0},
                       {4, 0, 8, 0, 0, 0, 0, 11, 0},
                       {0, 8, 0, 7, 0, 4, 0, 0, 2},
                       {0, 0, 7, 0, 9, 14, 0, 0, 0},
                       {0, 0, 0, 9, 0, 10, 0, 0, 0},
                       {0, 0, 4, 14, 10, 0, 2, 0, 0},
                       {0, 0, 0, 0, 0, 2, 0, 1, 6},
                       {8, 11, 0, 0, 0, 0, 1, 0, 7},
                       {0, 0, 2, 0, 0, 0, 6, 7, 0}};

    network_t *network = new_network(V);
    for (int i = 0; i < V; i++)
    {
        for (int j = 0; j < V; j++)
        {
            if (graph[i][j])
            {
                set_link_weight(network, i, j, graph[i][j]);
            }
        }
    }
    path_t *const *distances = weighted_distances(network, 0);
    for (int i = 0; i < V; i++)
    {
        printf("%ld:%ld\t", distances[i]->distance, distances[i]->length);
        for(int j = 0; j < distances[i]->length + 1; j++)
            printf("%ld;", distances[i]->nodes[j]);
        printf("\n");
    }
    printf("\n");

    distances = unweighted_distances(network, 0);
    for (int i = 0; i < V; i++)
    {
        printf("%ld:%ld\t", distances[i]->distance, distances[i]->length);
        for(int j = 0; j < distances[i]->length + 1; j++)
            printf("%ld;", distances[i]->nodes[j]);
        printf("\n");
    }
    printf("\n");
    free_network(network);
}
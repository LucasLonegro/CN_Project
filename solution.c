#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "network.h"
#include "problem.h"
#include "routing_solution.h"
#include "modulation_assignment.h"

int main(void)
{

    network_t *german_network = new_network(GERMAN_TOPOLOGY_SIZE);
    network_t *italian_network = new_network(ITALIAN_TOPOLOGY_SIZE);

    printf("=====================GERMAN=========================\n");
    for (uint64_t i = 0; i < (sizeof(german_links) / 3 / sizeof(uint64_t)); i++)
    {
        set_link_weight(german_network, german_links[i][0] - 1, german_links[i][1] - 1, german_links[i][2]);
    }

    path_t *const *german_paths = weighted_distances(german_network, 0);
    for (uint64_t i = 0; i < GERMAN_TOPOLOGY_SIZE; i++)
    {
        printf("1->%ld\t", i + 1);
        printf("%ld:%ld\t", german_paths[i]->distance, german_paths[i]->length);
        for (int j = 0; j < german_paths[i]->length + 1; j++)
            printf("%ld;", german_paths[i]->nodes[j] + 1);
        printf("\n");
    }
    printf("\n");
    printf("=====================ITALIAN=========================\n");
    for (uint64_t i = 0; i < (sizeof(italian_links) / 3 / sizeof(uint64_t)); i++)
    {
        set_link_weight(italian_network, italian_links[i][0] - 1, italian_links[i][1] - 1, italian_links[i][2]);
    }

    path_t *const *italian_paths = weighted_distances(italian_network, 0);
    for (uint64_t i = 0; i < ITALIAN_TOPOLOGY_SIZE; i++)
    {
        printf("1->%ld\t", i + 1);
        printf("%ld:%ld\t", italian_paths[i]->distance, italian_paths[i]->length);
        for (int j = 0; j < italian_paths[i]->length + 1; j++)
            printf("%ld;", italian_paths[i]->nodes[j] + 1);
        printf("\n");
    }
    printf("\n");

    connection_request r[ITALIAN_TOPOLOGY_SIZE * ITALIAN_TOPOLOGY_SIZE];
    uint64_t requests = 0;
    for (uint64_t i = 0; i < ITALIAN_TOPOLOGY_SIZE; i++)
    {
        for (uint64_t j = 0; j < ITALIAN_TOPOLOGY_SIZE; j++)
        {
            if (IT10_1[i][j] > 0)
            {
                r[requests].load = IT10_1[i][j];
                r[requests].from_node_id = i;
                r[requests].to_node_id = j;
                requests++;
            }
        }
    }
    routing_assignment a[ITALIAN_TOPOLOGY_SIZE * ITALIAN_TOPOLOGY_SIZE];
    generate_routing(italian_network, r, requests, a);
    for (uint64_t i = 0; i < requests; i++)
    {
        routing_assignment assignment = a[i];
        for (uint64_t j = 0; j <= assignment.path->length; j++)
        {
            printf("%ld", assignment.path->nodes[j] + 1);
            if (j != assignment.path->length)
                printf("->");
        }
        printf("\n");
        printf("Load: %ld\t", assignment.load);
        printf("Distance: %ld\t", assignment.path->distance);
    }

    uint64_t total_link_loads[ITALIAN_TOPOLOGY_SIZE * ITALIAN_TOPOLOGY_SIZE] = {0};
    for (uint64_t i = 0; i < requests; i++)
    {
        routing_assignment assignment = a[i];
        for (uint64_t node = 0; node < assignment.path->length; node++)
        {
            total_link_loads[assignment.path->nodes[node] * ITALIAN_TOPOLOGY_SIZE + assignment.path->nodes[node + 1]] += assignment.load;
        }
    }

    printf("\nTOTAL LINK LOADS\n");
    for (uint64_t i = 0; i < ITALIAN_TOPOLOGY_SIZE; i++)
    {
        for (uint64_t j = 0; j < ITALIAN_TOPOLOGY_SIZE; j++)
        {
            printf("%ld\t", total_link_loads[i * ITALIAN_TOPOLOGY_SIZE + j]);
        }
        printf("\n");
    }

    uint64_t assigned_formats[ITALIAN_TOPOLOGY_SIZE * ITALIAN_TOPOLOGY_SIZE * MODULATION_FORMATS_DIM] = {0};
    uint64_t *assigned_spectra = calloc(requests, sizeof(uint64_t));
    assign_modulation_formats_and_spectra(italian_network, formats, MODULATION_FORMATS_DIM, a, requests, assigned_formats, assigned_spectra);
    for (uint64_t i = 0; i < ITALIAN_TOPOLOGY_SIZE; i++)
    {
        for (uint64_t j = 0; j < ITALIAN_TOPOLOGY_SIZE; j++)
        {
            printf("{%ld, %ld, %ld}\t", assigned_formats[(i * ITALIAN_TOPOLOGY_SIZE + j) * MODULATION_FORMATS_DIM], assigned_formats[(i * ITALIAN_TOPOLOGY_SIZE + j) * MODULATION_FORMATS_DIM + 1], assigned_formats[(i * ITALIAN_TOPOLOGY_SIZE + j) * MODULATION_FORMATS_DIM + 2]);
        }
        printf("\n");
    }

    for (uint64_t i = 0; i < requests; i++)
    {
        printf("%ld->%ld, λ_%ld\n", a[i].path->nodes[0], a[i].path->nodes[a[i].path->length], assigned_spectra[i]);
    }

    free(assigned_spectra);

    free_network(german_network);
    free_network(italian_network);
}
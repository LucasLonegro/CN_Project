#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "network.h"
#include "problem.h"
#include "routing_solution.h"

#define COLOR_BOLD "\e[1m"
#define COLOR_BOLD_SLOW_BLINKING "\e[1;5m"
#define COLOR_BOLD_SLOW_BLINKING_RED "\e[1;5;31m"
#define COLOR_BOLD_BLUE "\e[1;34m"
#define COLOR_OFF "\e[m"

void free_assignment(assignment_t *assignment)
{
    if (assignment->is_split)
        free_assignment(assignment->split);
    free(assignment);
}

void print_assignment(const assignment_t assignment)
{
    for (uint64_t j = 0; j <= assignment.path->length; j++)
    {
        if (assignment.path->length == -1)
            break;
        printf("%ld", assignment.path->nodes[j] + 1);
        if (j != assignment.path->length)
            printf("->");
    }
    printf("\t");
    printf("Load: %ld\t", assignment.load);
    printf("Distance: %ld\t", assignment.path->distance);
    printf("Slots: %ld-%ld", assignment.start_slot, assignment.end_slot);
    if (assignment.is_split)
    {
        printf("\t[");
        print_assignment(*(assignment.split));
        printf("]\t");
    }
}

int main(void)
{
    network_t *italian_network = new_network(ITALIAN_TOPOLOGY_SIZE);

    for (uint64_t i = 0; i < (sizeof(italian_links) / 3 / sizeof(uint64_t)); i++)
    {
        set_link_weight(italian_network, italian_links[i][0] - 1, italian_links[i][1] - 1, italian_links[i][2]);
    }

    connection_request requests[ITALIAN_TOPOLOGY_SIZE * ITALIAN_TOPOLOGY_SIZE];
    uint64_t requests_count = 0;
    for (uint64_t i = 0; i < ITALIAN_TOPOLOGY_SIZE; i++)
    {
        for (uint64_t j = 0; j < ITALIAN_TOPOLOGY_SIZE; j++)
        {
            if (IT10_5[i][j] > 0)
            {
                requests[requests_count].load = IT10_5[i][j];
                requests[requests_count].from_node_id = i;
                requests[requests_count].to_node_id = j;
                requests_count++;
            }
        }
    }
    assignment_t *assignments = calloc(requests_count, sizeof(assignment_t));
    for (uint64_t i = 0; i < requests_count; i++)
    {
        assignments[i].is_split = 0;
        assignments[i].split = NULL;
    }

    dynamic_char_array assigned_formats[ITALIAN_TOPOLOGY_SIZE * ITALIAN_TOPOLOGY_SIZE] = {0};
    for(uint64_t i = 0; i < ITALIAN_TOPOLOGY_SIZE * ITALIAN_TOPOLOGY_SIZE; i++){
        assigned_formats[i].elements = NULL;
        assigned_formats[i].size = 0;
    }
    generate_routing(italian_network, requests, requests_count, formats, MODULATION_FORMATS_DIM, assignments, assigned_formats);
    for (uint64_t i = 0; i < requests_count; i++)
    {
        print_assignment(assignments[i]);
        printf("\n");
    }

    printf("\nTOTAL LINK LOADS\n");
    uint64_t total_link_loads[ITALIAN_TOPOLOGY_SIZE * ITALIAN_TOPOLOGY_SIZE] = {0};
    uint64_t link_fsus[ITALIAN_TOPOLOGY_SIZE * ITALIAN_TOPOLOGY_SIZE] = {0};
    for (uint64_t i = 0; i < requests_count; i++)
    {
        assignment_t assignment = assignments[i];
        for (uint64_t node = 0; node < assignment.path->length; node++)
        {
            if (assignment.path->length == -1)
                break;
            total_link_loads[assignment.path->nodes[node] * ITALIAN_TOPOLOGY_SIZE + assignment.path->nodes[node + 1]] += assignment.load;
            link_fsus[assignment.path->nodes[node] * ITALIAN_TOPOLOGY_SIZE + assignment.path->nodes[node + 1]]++;
        }
    }

    for (uint64_t i = 0; i < ITALIAN_TOPOLOGY_SIZE; i++)
    {
        for (uint64_t j = 0; j < ITALIAN_TOPOLOGY_SIZE; j++)
        {
            printf("%ld\t", total_link_loads[i * ITALIAN_TOPOLOGY_SIZE + j]);
        }
        printf("\n");
    }
    printf("\n");

    for (uint64_t i = 0; i < requests_count; i++)
    {
        if (assignments[i].is_split)
        {
            free_assignment(assignments[i].split);
        }
    }
    free(assignments);
    free_network(italian_network);
}
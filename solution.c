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

void run_solution(uint64_t topology_node_count, uint64_t links[][3], uint64_t links_count, const uint64_t *connection_requests, FILE *file)
{
    network_t *network = new_network(topology_node_count);

    for (uint64_t i = 0; i < links_count; i++)
    {
        set_link_weight(network, links[i][0] - 1, links[i][1] - 1, links[i][2]);
    }

    connection_request *requests = calloc(topology_node_count * topology_node_count, sizeof(connection_request));
    uint64_t requests_count = 0;
    for (uint64_t i = 0; i < topology_node_count; i++)
    {
        for (uint64_t j = 0; j < topology_node_count; j++)
        {
            if (connection_requests[i] > 0)
            {
                requests[requests_count].load = connection_requests[i * topology_node_count + j];
                requests[requests_count].from_node_id = i;
                requests[requests_count].to_node_id = j;
                requests_count++;
            }
        }
    }

    assignment_t *assignments = calloc(requests_count, sizeof(assignment_t));

    dynamic_char_array *used_frequency_slots = calloc(topology_node_count * topology_node_count, sizeof(dynamic_char_array));

    generate_routing(network, requests, requests_count, formats, MODULATION_FORMATS_DIM, assignments, used_frequency_slots);
    for (uint64_t i = 0; i < requests_count; i++)
    {
        print_assignment(assignments[i]);
        printf("\n");
    }

    printf("\nTOTAL LINK LOADS\n");
    uint64_t *total_link_loads = calloc(topology_node_count * topology_node_count, sizeof(uint64_t));
    for (uint64_t i = 0; i < requests_count; i++)
    {
        assignment_t assignment = assignments[i];
        for (uint64_t node = 0; node < assignment.path->length; node++)
        {
            if (assignment.path->length == -1)
                break;
            total_link_loads[assignment.path->nodes[node] * topology_node_count + assignment.path->nodes[node + 1]] += assignment.load;
        }
    }

    for (uint64_t i = 0; i < topology_node_count; i++)
    {
        for (uint64_t j = 0; j < topology_node_count; j++)
        {
            printf("%ld\t", total_link_loads[i * topology_node_count + j]);
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
    free(requests);
    free(total_link_loads);
    free(assignments);
    for (uint64_t i = 0; i < topology_node_count * topology_node_count; i++)
    {
        if (used_frequency_slots[i].size > 0)
            free(used_frequency_slots[i].elements);
    }
    free(used_frequency_slots);
    free_network(network);
}

int main(void)
{
    run_solution(ITALIAN_TOPOLOGY_SIZE, italian_links, ITALIAN_LINKS_SIZE, (uint64_t *)IT10_1, stdout);
    run_solution(ITALIAN_TOPOLOGY_SIZE, italian_links, ITALIAN_LINKS_SIZE, (uint64_t *)IT10_2, stdout);
    run_solution(ITALIAN_TOPOLOGY_SIZE, italian_links, ITALIAN_LINKS_SIZE, (uint64_t *)IT10_3, stdout);
    run_solution(ITALIAN_TOPOLOGY_SIZE, italian_links, ITALIAN_LINKS_SIZE, (uint64_t *)IT10_4, stdout);
    run_solution(ITALIAN_TOPOLOGY_SIZE, italian_links, ITALIAN_LINKS_SIZE, (uint64_t *)IT10_5, stdout);
    return 0;
}
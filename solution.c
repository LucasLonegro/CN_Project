#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
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

void print_assignment(const assignment_t assignment, FILE *file)
{
    for (uint64_t j = 0; j <= assignment.path->length; j++)
    {
        if (assignment.path->length == -1)
            break;
        fprintf(file, "%ld", assignment.path->nodes[j] + 1);
        if (j != assignment.path->length)
            fprintf(file, "->");
    }
    fprintf(file, "\t");
    fprintf(file, "Load: %ld\t", assignment.load);
    fprintf(file, "Distance: %ld\t", assignment.path->distance);
    fprintf(file, "Slots: %ld-%ld", assignment.start_slot, assignment.end_slot);
    if (assignment.is_split)
    {
        fprintf(file, "\t[");
        print_assignment(*(assignment.split), file);
        fprintf(file, "]\t");
    }
}

__ssize_t highest_fsu_in_assignent(assignment_t *assignment)
{
    if (assignment->is_split)
    {
        uint64_t split_slot = highest_fsu_in_assignent(assignment->split);
        if (split_slot > assignment->end_slot)
            return split_slot;
    }
    return assignment->end_slot;
}

double utilization_entropy(const dynamic_char_array *frequency_slots)
{
    if (frequency_slots->size <= 0)
        return 0;
    uint64_t status_changes = 0;
    int status = get_element(frequency_slots, 0) == USED ? USED : UNUSED;
    for (uint64_t i = 0; i < frequency_slots->size; i++)
    {
        int slot_status = get_element(frequency_slots, i) == USED ? USED : UNUSED;
        if (slot_status != status)
        {
            status = slot_status;
            status_changes++;
        }
    }
    return (double)status_changes / (MAX_SPECTRAL_SLOTS - 1);
}

double shannon_entropy(const dynamic_char_array *frequency_slots)
{
    if (frequency_slots->size <= 0)
        return 0;
    double entropy = 0;
    uint64_t status_changes = 0;
    uint64_t block_size = 0;
    int status = get_element(frequency_slots, 0) == USED ? USED : UNUSED;

    for (uint64_t i = 0; i < frequency_slots->size; i++)
    {
        int slot_status = get_element(frequency_slots, i) == USED ? USED : UNUSED;
        if (slot_status != status)
        {
            if (slot_status == UNUSED)
                entropy += ((double)block_size / MAX_SPECTRAL_SLOTS) * log((double)block_size / MAX_SPECTRAL_SLOTS);
            status = slot_status;
            status_changes++;
            block_size = 0;
        }
        block_size++;
    }

    return -entropy;
}

void bubble_sort_connection_requests(connection_request *requests, uint64_t requests_dim, int ascending)
{
    uint64_t sorted = 0;
    while (!sorted)
    {
        sorted = 1;
        for (uint64_t i = 0; i < requests_dim - 1; i++)
        {
            if ((ascending && requests[i].load > requests[i + 1].load) || (!ascending && requests[i].load < requests[i + 1].load))
            {
                sorted = 0;
                connection_request aux = requests[i];
                requests[i] = requests[i + 1];
                requests[i + 1] = aux;
            }
        }
    }
}

void run_solution(uint64_t topology_node_count, uint64_t links[][3], uint64_t links_count, const uint64_t *connection_requests, int ascending, FILE *file)
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
            if (connection_requests[i] > 0 && i != j)
            {
                requests[requests_count].load = connection_requests[i * topology_node_count + j];
                requests[requests_count].from_node_id = i;
                requests[requests_count].to_node_id = j;
                requests_count++;
            }
        }
    }
    bubble_sort_connection_requests(requests, requests_count, ascending);

    assignment_t *assignments = calloc(requests_count, sizeof(assignment_t));

    dynamic_char_array *used_frequency_slots = calloc(topology_node_count * topology_node_count, sizeof(dynamic_char_array));

    generate_routing(network, requests, requests_count, formats, MODULATION_FORMATS_DIM, assignments, used_frequency_slots, FIXED_SHORTEST_PATH, FIRST_FIT_SLOT, DEFAULT);

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

    fprintf(file, COLOR_BOLD_BLUE "\nRMSAs\n" COLOR_OFF);
    for (uint64_t i = 0; i < requests_count; i++)
    {
        print_assignment(assignments[i], file);
        fprintf(file, "\n");
    }

    fprintf(file, COLOR_BOLD_BLUE "\nPERFORMANCE PARAMENTERS\n" COLOR_OFF);
    __ssize_t highest_fsu = -1;
    for (uint64_t i = 0; i < requests_count; i++)
    {
        __ssize_t slot = highest_fsu_in_assignent(assignments + i);
        if (slot > highest_fsu)
            highest_fsu = slot;
    }
    fprintf(file, COLOR_BOLD_BLUE "Highest used fsu: %ld\n" COLOR_OFF, highest_fsu);

    __ssize_t highest_loaded_link = -1;
    for (uint64_t i = 0; i < topology_node_count * topology_node_count; i++)
    {
        if (total_link_loads[i] > highest_loaded_link || highest_loaded_link == -1)
            highest_loaded_link = total_link_loads[i];
    }
    fprintf(file, COLOR_BOLD_BLUE "Highest link load: %ld\n" COLOR_OFF, highest_loaded_link);

    fprintf(file, "Link usage distribution:\n");
    for (uint64_t i = 0; i < topology_node_count; i++)
    {
        for (uint64_t j = 0; j < topology_node_count; j++)
        {
            if (get_link_weight(network, i, j) > 0)
                fprintf(file, "%ld\t", total_link_loads[i * topology_node_count + j]);
            else
                fprintf(file, "■\t");
        }
        fprintf(file, "\n");
    }
    fprintf(file, "\n");

    fprintf(file, COLOR_BOLD_BLUE "Path length distribution:\n" COLOR_OFF);
    for (uint64_t i = 0, request_index = 0; i < topology_node_count; i++)
    {
        for (uint64_t j = 0; j < topology_node_count && request_index < requests_count; j++)
        {
            if (links[i * topology_node_count + j] > 0 && i != j)
            {
                fprintf(file, "%ld", assignments[request_index].path->distance);
                if (assignments[request_index].is_split)
                {
                    fprintf(file, " [%ld]", assignments[request_index].path->distance);
                }
                else
                {
                    fprintf(file, "\t");
                }
                fprintf(file, "\t");
                request_index++;
            }
            else
            {
                printf("\t\t");
            }
        }
        fprintf(file, "\n");
    }

    fprintf(file, COLOR_BOLD_BLUE "Utilization entropies [thousandths]:\n" COLOR_OFF);
    for (uint64_t i = 0; i < topology_node_count; i++)
    {
        for (uint64_t j = 0; j < topology_node_count; j++)
        {
            if (get_link_weight(network, i, j) != -1)
            {
                fprintf(file, "%.2f\t", utilization_entropy(used_frequency_slots + i * topology_node_count + j) * 1000);
            }
            else
            {
                printf("\t");
            }
        }
        fprintf(file, "\n");
    }
    fprintf(file, "\n");

    fprintf(file, COLOR_BOLD_BLUE "Shannon entropies:\n" COLOR_OFF);
    for (uint64_t i = 0; i < topology_node_count; i++)
    {
        for (uint64_t j = 0; j < topology_node_count; j++)
        {
            if (get_link_weight(network, i, j) != -1)
            {
                fprintf(file, "%.2f\t", shannon_entropy(used_frequency_slots + i * topology_node_count + j));
            }
            else
            {
                printf("\t");
            }
        }
        fprintf(file, "\n");
    }
    fprintf(file, "\n");

    fprintf(file, "\n");

    fprintf(file, "\nEND\n");

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
    run_solution(GERMAN_TOPOLOGY_SIZE, german_links, GERMAN_LINKS_SIZE, (uint64_t *)g7_1, 0, stdout);
    run_solution(GERMAN_TOPOLOGY_SIZE, german_links, GERMAN_LINKS_SIZE, (uint64_t *)g7_1, 1, stdout);
    run_solution(GERMAN_TOPOLOGY_SIZE, german_links, GERMAN_LINKS_SIZE, (uint64_t *)g7_2, 0, stdout);
    run_solution(GERMAN_TOPOLOGY_SIZE, german_links, GERMAN_LINKS_SIZE, (uint64_t *)g7_2, 1, stdout);
    run_solution(GERMAN_TOPOLOGY_SIZE, german_links, GERMAN_LINKS_SIZE, (uint64_t *)g7_3, 0, stdout);
    run_solution(GERMAN_TOPOLOGY_SIZE, german_links, GERMAN_LINKS_SIZE, (uint64_t *)g7_3, 1, stdout);
    run_solution(GERMAN_TOPOLOGY_SIZE, german_links, GERMAN_LINKS_SIZE, (uint64_t *)g7_4, 0, stdout);
    run_solution(GERMAN_TOPOLOGY_SIZE, german_links, GERMAN_LINKS_SIZE, (uint64_t *)g7_4, 1, stdout);
    run_solution(GERMAN_TOPOLOGY_SIZE, german_links, GERMAN_LINKS_SIZE, (uint64_t *)g7_5, 0, stdout);
    run_solution(GERMAN_TOPOLOGY_SIZE, german_links, GERMAN_LINKS_SIZE, (uint64_t *)g7_5, 1, stdout);

    run_solution(ITALIAN_TOPOLOGY_SIZE, italian_links, ITALIAN_LINKS_SIZE, (uint64_t *)IT10_1, 0, stdout);
    run_solution(ITALIAN_TOPOLOGY_SIZE, italian_links, ITALIAN_LINKS_SIZE, (uint64_t *)IT10_1, 1, stdout);
    run_solution(ITALIAN_TOPOLOGY_SIZE, italian_links, ITALIAN_LINKS_SIZE, (uint64_t *)IT10_2, 0, stdout);
    run_solution(ITALIAN_TOPOLOGY_SIZE, italian_links, ITALIAN_LINKS_SIZE, (uint64_t *)IT10_2, 1, stdout);
    run_solution(ITALIAN_TOPOLOGY_SIZE, italian_links, ITALIAN_LINKS_SIZE, (uint64_t *)IT10_3, 0, stdout);
    run_solution(ITALIAN_TOPOLOGY_SIZE, italian_links, ITALIAN_LINKS_SIZE, (uint64_t *)IT10_3, 1, stdout);
    run_solution(ITALIAN_TOPOLOGY_SIZE, italian_links, ITALIAN_LINKS_SIZE, (uint64_t *)IT10_4, 0, stdout);
    run_solution(ITALIAN_TOPOLOGY_SIZE, italian_links, ITALIAN_LINKS_SIZE, (uint64_t *)IT10_4, 1, stdout);
    run_solution(ITALIAN_TOPOLOGY_SIZE, italian_links, ITALIAN_LINKS_SIZE, (uint64_t *)IT10_5, 0, stdout);
    run_solution(ITALIAN_TOPOLOGY_SIZE, italian_links, ITALIAN_LINKS_SIZE, (uint64_t *)IT10_5, 1, stdout);
    return 0;
}
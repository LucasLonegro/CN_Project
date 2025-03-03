#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "network.h"
#include "problem.h"
#include "routing_solution.h"
#include "graph_drawing.h"

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
    if (assignment.path == NULL || assignment.path->length == -1)
    {
        fprintf(file, COLOR_BOLD_SLOW_BLINKING_RED "Unassigned Connection" COLOR_OFF);
        return;
    }
    for (uint64_t j = 0; j <= assignment.path->length; j++)
    {
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

typedef struct read_link_weight_data_t
{
    char buffer[256];
    const network_t *network;
} read_link_weight_data_t;

char *read_link_weight(void *data, uint64_t from, uint64_t to)
{
    read_link_weight_data_t *_data = (read_link_weight_data_t *)data;
    sprintf(_data->buffer, "%ld", get_link_weight(_data->network, from, to));
    return _data->buffer;
}

void run_requests(const network_t *network, const uint64_t *connection_requests, int ascending, FILE *file)
{
    uint64_t topology_node_count = network->node_count;
    connection_request *requests = calloc(topology_node_count * topology_node_count, sizeof(connection_request));
    uint64_t requests_count = 0;
    for (uint64_t i = 0; i < topology_node_count; i++)
    {
        for (uint64_t j = 0; j < topology_node_count; j++)
        {
            if (connection_requests[i * topology_node_count + j] > 0 && i != j)
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
    assignment_t *protections = calloc(requests_count, sizeof(assignment_t));

    dynamic_char_array *used_frequency_slots = calloc(topology_node_count * topology_node_count, sizeof(dynamic_char_array));

    path_t **to_free = generate_routing(network, requests, requests_count, formats, MODULATION_FORMATS_DIM, assignments, protections, SHARED_PROTECTION, used_frequency_slots, LEAST_USED_PATH_JOINT, FIRST_FIT_SLOT, DEFAULT_MODULATION);

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

    fprintf(file, COLOR_BOLD_BLUE "\nProtection RMSAs\n" COLOR_OFF);
    for (uint64_t i = 0; i < requests_count; i++)
    {
        print_assignment(protections[i], file);
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

    mkdir("./reports", S_IRWXU | S_IRWXG | S_IROTH);
    FILE *f = fopen("reports/out.tex", "w+");
    print_prologue(f, "italian");
    print_section(f, "Topology");
    coordinate c[] = {{.x = 2.0, .y = 4.0}, {.x = 3.5, .y = 4.0}, {.x = 0.5, .y = 4.0}, {.x = 5.0, .y = 4.0}, {.x = 0.0, .y = 2.0}, {.x = 0.5, .y = 0.0}, {.x = 2.0, .y = 2.0}, {.x = 4.0, .y = 2.0}, {.x = 2.8, .y = 0.0}, {.x = 4.0, .y = 0.0}};
    read_link_weight_data_t data = {.network = network};
    print_graph(network, read_link_weight, &data, c, NULL, 0, 1.5, 1.0, f);
    data_point points[1][4] = {{{.x = "a", .y = 10}, {.x = "b", .y = 20}, {.x = "c", .y = 22}, {.x = "d", .y = 11}}};
    char *x_labels[] = {"a", "b","c","d"};
    char *legends[] = {"Range"};
    bar_plot p = {.data_points_count = 4, .samples_count = 1, .legends = legends, .y_label = "Frequency", .data_points = (data_point *)points, .x_labels_count = 4, .x_labels = x_labels};
    print_section(f, "Entropy");
    print_plot(p, f);
    print_epilogue(f);
    fclose(f);

    for (uint64_t i = 0; i < requests_count; i++)
    {
        if (assignments[i].is_split)
        {
            free_assignment(assignments[i].split);
        }
        if (protections[i].is_split)
        {
            free_assignment(protections[i].split);
        }
    }
    free(requests);
    free(total_link_loads);
    free(assignments);
    free(protections);
    for (uint64_t i = 0; to_free != NULL && to_free[i] != NULL; i++)
    {
        free(to_free[i]->nodes);
        free(to_free[i]);
    }
    free(to_free);
    for (uint64_t i = 0; i < topology_node_count * topology_node_count; i++)
    {
        if (used_frequency_slots[i].size > 0)
            free(used_frequency_slots[i].elements);
    }
    free(used_frequency_slots);
}

network_t *build_network(uint64_t topology_node_count, uint64_t links[][3], uint64_t links_count)
{
    network_t *network = new_network(topology_node_count);

    for (uint64_t i = 0; i < links_count; i++)
    {
        set_link_weight(network, links[i][0] - 1, links[i][1] - 1, links[i][2]);
    }

    return network;
}

void run_solutions()
{
    network_t *german_n = build_network(GERMAN_TOPOLOGY_SIZE, german_links, GERMAN_LINKS_SIZE);
    // run_requests(german_n, (uint64_t *)g7_1, 0, stdout);
    // run_requests(german_n, (uint64_t *)g7_1, 1, stdout);
    // run_requests(german_n, (uint64_t *)g7_2, 0, stdout);
    // run_requests(german_n, (uint64_t *)g7_2, 1, stdout);
    // run_requests(german_n, (uint64_t *)g7_3, 0, stdout);
    // run_requests(german_n, (uint64_t *)g7_3, 1, stdout);
    // run_requests(german_n, (uint64_t *)g7_4, 0, stdout);
    // run_requests(german_n, (uint64_t *)g7_4, 1, stdout);
    // run_requests(german_n, (uint64_t *)g7_5, 0, stdout);
    // run_requests(german_n, (uint64_t *)g7_5, 1, stdout);
    free_network(german_n);
    network_t *italian_n = build_network(ITALIAN_TOPOLOGY_SIZE, italian_links, ITALIAN_LINKS_SIZE);
    // run_requests(italian_n, (uint64_t *)IT10_1, 0, stdout);
    // run_requests(italian_n, (uint64_t *)IT10_1, 1, stdout);
    // run_requests(italian_n, (uint64_t *)IT10_2, 0, stdout);
    // run_requests(italian_n, (uint64_t *)IT10_2, 1, stdout);
    // run_requests(italian_n, (uint64_t *)IT10_3, 0, stdout);
    // run_requests(italian_n, (uint64_t *)IT10_3, 1, stdout);
    // run_requests(italian_n, (uint64_t *)IT10_4, 0, stdout);
    // run_requests(italian_n, (uint64_t *)IT10_4, 1, stdout);
    // run_requests(italian_n, (uint64_t *)IT10_5, 0, stdout);
    run_requests(italian_n, (uint64_t *)IT10_5, 1, stdout);
    free_network(italian_n);
}

int main(void)
{
    // run_solution(GERMAN_TOPOLOGY_SIZE, german_links, GERMAN_LINKS_SIZE, (uint64_t *)g7_1, 0, stdout);
    // run_solution(GERMAN_TOPOLOGY_SIZE, german_links, GERMAN_LINKS_SIZE, (uint64_t *)g7_1, 1, stdout);
    // run_solution(GERMAN_TOPOLOGY_SIZE, german_links, GERMAN_LINKS_SIZE, (uint64_t *)g7_2, 0, stdout);
    // run_solution(GERMAN_TOPOLOGY_SIZE, german_links, GERMAN_LINKS_SIZE, (uint64_t *)g7_2, 1, stdout);
    // run_solution(GERMAN_TOPOLOGY_SIZE, german_links, GERMAN_LINKS_SIZE, (uint64_t *)g7_3, 0, stdout);
    // run_solution(GERMAN_TOPOLOGY_SIZE, german_links, GERMAN_LINKS_SIZE, (uint64_t *)g7_3, 1, stdout);
    // run_solution(GERMAN_TOPOLOGY_SIZE, german_links, GERMAN_LINKS_SIZE, (uint64_t *)g7_4, 0, stdout);
    // run_solution(GERMAN_TOPOLOGY_SIZE, german_links, GERMAN_LINKS_SIZE, (uint64_t *)g7_4, 1, stdout);
    // run_solution(GERMAN_TOPOLOGY_SIZE, german_links, GERMAN_LINKS_SIZE, (uint64_t *)g7_5, 0, stdout);
    // run_solution(GERMAN_TOPOLOGY_SIZE, german_links, GERMAN_LINKS_SIZE, (uint64_t *)g7_5, 1, stdout);
    //
    // run_solution(ITALIAN_TOPOLOGY_SIZE, italian_links, ITALIAN_LINKS_SIZE, (uint64_t *)IT10_1, 0, stdout);
    // run_solution(ITALIAN_TOPOLOGY_SIZE, italian_links, ITALIAN_LINKS_SIZE, (uint64_t *)IT10_1, 1, stdout);
    // run_solution(ITALIAN_TOPOLOGY_SIZE, italian_links, ITALIAN_LINKS_SIZE, (uint64_t *)IT10_2, 0, stdout);
    // run_solution(ITALIAN_TOPOLOGY_SIZE, italian_links, ITALIAN_LINKS_SIZE, (uint64_t *)IT10_2, 1, stdout);
    // run_solution(ITALIAN_TOPOLOGY_SIZE, italian_links, ITALIAN_LINKS_SIZE, (uint64_t *)IT10_3, 0, stdout);
    // run_solution(ITALIAN_TOPOLOGY_SIZE, italian_links, ITALIAN_LINKS_SIZE, (uint64_t *)IT10_3, 1, stdout);
    // run_solution(ITALIAN_TOPOLOGY_SIZE, italian_links, ITALIAN_LINKS_SIZE, (uint64_t *)IT10_4, 0, stdout);
    // run_solution(ITALIAN_TOPOLOGY_SIZE, italian_links, ITALIAN_LINKS_SIZE, (uint64_t *)IT10_4, 1, stdout);
    // run_solution(ITALIAN_TOPOLOGY_SIZE, italian_links, ITALIAN_LINKS_SIZE, (uint64_t *)IT10_5, 0, stdout);
    run_solutions();
    return 0;
}
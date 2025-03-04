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

#define LINK_USAGE_MAX_PLOT 2000
#define LINK_USAGE_PLOT_INTERVAL 100

#define PATH_LENGTH_MAX_PLOT 2000
#define PATH_LENGTH_PLOT_INTERVAL 200
#define BAR_PLOT_WIDTH 12
#define BIG_BAR_PLOT_WIDTH 18

#define ENTROPY_MAX_PLOT 1.0
#define ENTROPY_PLOT_INTERVAL 0.01
#define ENTROPY_PLOT_INTERVALS_COUNT 100
#define SHANNON_ENTROPY_PLOT_INTERVAL 0.05
#define SHANNON_ENTROPY_PLOT_INTERVALS_COUNT 20

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

char *noop_labeler(const void *data, uint64_t from, uint64_t to)
{
    return "";
}

hex_color get_hex_color(assignment_t assignment)
{
    hex_color ret = {
        .blue = (assignment.start_slot * ((double)127 / MAX_SPECTRAL_SLOTS)) + 128,
        .red = ((double)assignment.start_slot / MAX_SPECTRAL_SLOTS) / 127 ? ((double)assignment.start_slot / MAX_SPECTRAL_SLOTS) * 127 : 255 - ((double)assignment.start_slot / MAX_SPECTRAL_SLOTS) * 127,
        .green = 255 - (assignment.start_slot * ((double)127 / MAX_SPECTRAL_SLOTS))};
    return ret;
}

typedef struct read_slots_data_t
{
    char buffer[256];
    const assignment_t *assignment;
} read_slots_data_t;

char *read_slots(const void *data, uint64_t from, uint64_t to)
{
    read_slots_data_t *_data = (read_slots_data_t *)data;
    if (_data->assignment == NULL || _data->assignment->path == NULL || _data->assignment->path->length == -1)
        return "";
    if (from == _data->assignment->path->nodes[_data->assignment->path->length / 2])
    {
        snprintf(_data->buffer, sizeof(_data->buffer), "\\(\\{\\lambda_{%ld};\\lambda_{%ld}\\}\\)", _data->assignment->start_slot, _data->assignment->end_slot);
        return _data->buffer;
    }
    return "";
}

void print_assignment_latex(const network_t *network, assignment_t *assignment, coordinate *coordinates, assignment_t *protection, FILE *file)
{
    read_slots_data_t data1 = {.assignment = assignment};
    read_slots_data_t data2 = {.assignment = protection};
    path_drawing assignment_path = {.line = SOLID_LINE, .offset = OFFSET_DOWN, .path = assignment->path, .color = get_hex_color(*assignment), .data = &data1, .labeler = read_slots};
    path_drawing protection_path = {.line = DOTTED_LINE, .offset = OFFSET_DOWN, .path = protection->path, .color = get_hex_color(*protection), .data = &data2, .labeler = read_slots};
    path_drawing paths[] = {assignment_path, protection_path};
    print_graph(network, noop_labeler, NULL, coordinates, paths, protection->path->length == -1 ? 1 : 2, 0.9, 0.8, file, 0);
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

char *read_link_weight(const void *data, uint64_t from, uint64_t to)
{
    read_link_weight_data_t *_data = (read_link_weight_data_t *)data;
    snprintf(_data->buffer, sizeof(_data->buffer), "%ld", get_link_weight(_data->network, from, to));
    return _data->buffer;
}

void print_link_usage(uint64_t topology_node_count, uint64_t *total_link_loads, FILE *f)
{
    data_point points[LINK_USAGE_MAX_PLOT / LINK_USAGE_PLOT_INTERVAL + 1][1] = {};
    char *link_loads_x_labels[LINK_USAGE_MAX_PLOT / LINK_USAGE_PLOT_INTERVAL + 1];
    uint64_t index = 0;
    for (uint64_t i = 0; i < LINK_USAGE_MAX_PLOT; i += LINK_USAGE_PLOT_INTERVAL)
    {
        link_loads_x_labels[index] = calloc(16, sizeof(char));
        points[index][0].x = link_loads_x_labels[index];
        snprintf(link_loads_x_labels[index], 16, "%ld-%ld", i, i + LINK_USAGE_PLOT_INTERVAL);
        index++;
    }
    char *legends[] = {"Load Range"};
    bar_plot p = {.width = BAR_PLOT_WIDTH, .samples_count = 1, .legends = legends, .y_label = "Frequency", .data_points = (data_point *)points, .x_labels_count = (LINK_USAGE_MAX_PLOT / LINK_USAGE_PLOT_INTERVAL), .x_labels = link_loads_x_labels};
    uint64_t link_usages[LINK_USAGE_MAX_PLOT / LINK_USAGE_PLOT_INTERVAL + 1];
    for (uint64_t link_index = 0; link_index < topology_node_count * topology_node_count; link_index++)
    {
        uint64_t index = 0;
        if (total_link_loads[link_index] > 0)
        {
            for (uint64_t i = 0; i < LINK_USAGE_MAX_PLOT; i += LINK_USAGE_PLOT_INTERVAL)
            {
                if (total_link_loads[link_index] < i)
                {
                    points[index - 1][0].y++;
                    link_usages[index - 1]++;
                    if (index > p.data_points_count)
                        p.data_points_count = index + (index == (LINK_USAGE_MAX_PLOT / LINK_USAGE_PLOT_INTERVAL) ? 0 : 1);
                    break;
                }
                index++;
            }
        }
    }
    print_plot(p, f);
    index = 0;
    for (uint64_t i = 0; i < LINK_USAGE_MAX_PLOT; i += LINK_USAGE_PLOT_INTERVAL)
    {
        free(link_loads_x_labels[index++]);
    }
}

void print_path_lengths(uint64_t topology_node_count, assignment_t *assignments, uint64_t request_count, FILE *f)
{
    data_point points[PATH_LENGTH_MAX_PLOT / PATH_LENGTH_PLOT_INTERVAL + 1][1] = {};
    char *link_loads_x_labels[PATH_LENGTH_MAX_PLOT / PATH_LENGTH_PLOT_INTERVAL + 1];
    uint64_t index = 0;
    for (uint64_t i = 0; i < PATH_LENGTH_MAX_PLOT; i += PATH_LENGTH_PLOT_INTERVAL)
    {
        link_loads_x_labels[index] = calloc(16, sizeof(char));
        points[index][0].x = link_loads_x_labels[index];
        snprintf(link_loads_x_labels[index], 16, "%ld-%ld", i, i + PATH_LENGTH_PLOT_INTERVAL);
        index++;
    }
    char *legends[] = {"Length Range"};
    bar_plot p = {.width = BAR_PLOT_WIDTH, .samples_count = 1, .legends = legends, .y_label = "Frequency", .data_points = (data_point *)points, .x_labels_count = (PATH_LENGTH_MAX_PLOT / PATH_LENGTH_PLOT_INTERVAL), .x_labels = link_loads_x_labels};
    uint64_t link_usages[PATH_LENGTH_MAX_PLOT / PATH_LENGTH_PLOT_INTERVAL + 1];
    for (uint64_t assignment_index = 0; assignment_index < request_count; assignment_index++)
    {
        uint64_t index = 0;
        if (assignments[assignment_index].path->length > 0)
        {
            for (uint64_t i = 0; i < PATH_LENGTH_MAX_PLOT; i += PATH_LENGTH_PLOT_INTERVAL)
            {
                if (assignments[assignment_index].path->distance < i)
                {
                    points[index - 1][0].y++;
                    link_usages[index - 1]++;
                    if (index > p.data_points_count)
                        p.data_points_count = index + (index == (PATH_LENGTH_MAX_PLOT / PATH_LENGTH_PLOT_INTERVAL) ? 0 : 1);
                    break;
                }
                index++;
            }
        }
    }
    print_plot(p, f);
    index = 0;
    for (uint64_t i = 0; i < PATH_LENGTH_MAX_PLOT; i += PATH_LENGTH_PLOT_INTERVAL)
    {
        free(link_loads_x_labels[index++]);
    }
}

typedef struct read_entropy_data_t
{
    char buffer[256];
    const network_t *network;
    const dynamic_char_array *used_frequency_slots;
    double (*entropy_calculator)(const dynamic_char_array *);
} read_entropy_data_t;

char *read_entropy(const void *data, uint64_t from, uint64_t to)
{
    read_entropy_data_t *_data = (read_entropy_data_t *)data;
    snprintf(_data->buffer, sizeof(_data->buffer), "%.3f", _data->entropy_calculator(_data->used_frequency_slots + from * _data->network->node_count + to));
    return _data->buffer;
}

void print_entropy(const network_t *network, coordinate *coordinates, uint64_t topology_node_count, FILE *f, dynamic_char_array *used_frequency_slots)
{
    read_entropy_data_t data = {.used_frequency_slots = used_frequency_slots, .network = network, .entropy_calculator = utilization_entropy};
    print_graph(network, read_entropy, &data, coordinates, NULL, 0, 2.0, 1.5, f, DRAW_ARROWS);
    fprintf(f, "\n");
    print_graph(network, read_entropy, &data, coordinates, NULL, 0, 2.0, 1.5, f, DRAW_ARROWS | REVERSE_ARROWS);
    data_point points[ENTROPY_PLOT_INTERVALS_COUNT][1] = {};
    char *link_loads_x_labels[ENTROPY_PLOT_INTERVALS_COUNT];
    uint64_t index = 0;
    for (double i = 0; i < ENTROPY_MAX_PLOT; i += ENTROPY_PLOT_INTERVAL)
    {
        link_loads_x_labels[index] = calloc(16, sizeof(char));
        points[index][0].x = link_loads_x_labels[index];
        snprintf(link_loads_x_labels[index], 16, "%.2f-%.2f", i, i + ENTROPY_PLOT_INTERVAL);
        index++;
    }
    char *legends[] = {"Entropy Range"};
    bar_plot p = {.width = BIG_BAR_PLOT_WIDTH, .samples_count = 1, .legends = legends, .y_label = "Frequency", .data_points = (data_point *)points, .x_labels_count = (ENTROPY_PLOT_INTERVALS_COUNT), .x_labels = link_loads_x_labels};
    uint64_t link_usages[ENTROPY_PLOT_INTERVALS_COUNT];
    for (uint64_t from = 0; from < topology_node_count; from++)
    {
        for (uint64_t to = 0; to < topology_node_count; to++)
        {
            uint64_t index = 0;
            if (get_link_weight(network, from, to) != -1)
            {
                double entropy = utilization_entropy(used_frequency_slots + from * topology_node_count + to);

                for (double i = 0; i < ENTROPY_MAX_PLOT; i += ENTROPY_PLOT_INTERVAL)
                {
                    if (entropy < i)
                    {
                        points[index - 1][0].y++;
                        link_usages[index - 1]++;
                        if (index > p.data_points_count)
                            p.data_points_count = index + (index == (ENTROPY_PLOT_INTERVALS_COUNT) ? 0 : 1);
                        break;
                    }
                    index++;
                }
            }
        }
    }
    print_plot(p, f);
    for (uint64_t i = 0; i < ENTROPY_PLOT_INTERVALS_COUNT; i++)
    {
        free(link_loads_x_labels[i]);
    }
}

void print_shannon_entropy(const network_t *network, coordinate *coordinates, uint64_t topology_node_count, FILE *f, dynamic_char_array *used_frequency_slots)
{
    read_entropy_data_t data = {.used_frequency_slots = used_frequency_slots, .network = network, .entropy_calculator = shannon_entropy};
    print_graph(network, read_entropy, &data, coordinates, NULL, 0, 2.0, 1.5, f, DRAW_ARROWS);
    fprintf(f, "\n");
    print_graph(network, read_entropy, &data, coordinates, NULL, 0, 2.0, 1.5, f, DRAW_ARROWS | REVERSE_ARROWS);
    data_point points[SHANNON_ENTROPY_PLOT_INTERVALS_COUNT][1] = {};
    char *link_loads_x_labels[SHANNON_ENTROPY_PLOT_INTERVALS_COUNT];
    uint64_t index = 0;
    for (double i = 0; i < ENTROPY_MAX_PLOT; i += SHANNON_ENTROPY_PLOT_INTERVAL)
    {
        link_loads_x_labels[index] = calloc(16, sizeof(char));
        points[index][0].x = link_loads_x_labels[index];
        snprintf(link_loads_x_labels[index], 16, "%.3f-%.3f", i, i + SHANNON_ENTROPY_PLOT_INTERVAL);
        index++;
    }
    char *legends[] = {"Entropy Range"};
    bar_plot p = {.width = BIG_BAR_PLOT_WIDTH, .samples_count = 1, .legends = legends, .y_label = "Frequency", .data_points = (data_point *)points, .x_labels_count = (SHANNON_ENTROPY_PLOT_INTERVALS_COUNT), .x_labels = link_loads_x_labels};
    uint64_t link_usages[SHANNON_ENTROPY_PLOT_INTERVALS_COUNT];
    for (uint64_t from = 0; from < topology_node_count; from++)
    {
        for (uint64_t to = 0; to < topology_node_count; to++)
        {
            uint64_t index = 0;
            if (get_link_weight(network, from, to) != -1)
            {
                double entropy = shannon_entropy(used_frequency_slots + from * topology_node_count + to);

                for (double i = 0; i < ENTROPY_MAX_PLOT; i += SHANNON_ENTROPY_PLOT_INTERVAL)
                {
                    if (entropy < i)
                    {
                        points[index - 1][0].y++;
                        link_usages[index - 1]++;
                        if (index > p.data_points_count)
                            p.data_points_count = index + (index == (SHANNON_ENTROPY_PLOT_INTERVALS_COUNT) ? 0 : 1);
                        break;
                    }
                    index++;
                }
            }
        }
    }
    print_plot(p, f);
    for (uint64_t i = 0; i < SHANNON_ENTROPY_PLOT_INTERVALS_COUNT; i++)
    {
        free(link_loads_x_labels[i]);
    }
}

void print_report(const network_t *network, assignment_t *assignments, uint64_t requests_count, assignment_t *protections, dynamic_char_array *used_frequency_slots, uint64_t *total_link_loads, char *title, FILE *f, coordinate *coordinates, protection_type protection)
{
    print_prologue(f, title);

    print_section(f, "Topology");
    read_link_weight_data_t data = {.network = network};
    print_graph(network, read_link_weight, &data, coordinates, NULL, 0, 1.5, 1.0, f, 0);

    uint64_t topology_node_count = network->node_count;

    __ssize_t highest_loaded_link = -1;
    for (uint64_t i = 0; i < topology_node_count * topology_node_count; i++)
    {
        if (total_link_loads[i] > highest_loaded_link || highest_loaded_link == -1)
            highest_loaded_link = total_link_loads[i];
    }
    __ssize_t highest_fsu = -1;
    for (uint64_t i = 0; i < requests_count; i++)
    {
        __ssize_t slot = highest_fsu_in_assignent(assignments + i);
        if (slot > highest_fsu)
            highest_fsu = slot;
    }
    uint64_t total_fsus = 0;
    for (uint64_t i = 0; i < requests_count; i++)
    {
        total_fsus += assignments[i].end_slot - assignments[i].start_slot;
    }
    uint64_t format_frequencies[MODULATION_FORMATS_DIM] = {};
    uint64_t transponder_cost = 0;
    for (uint64_t i = 0; i < requests_count; i++)
    {
        assignment_t *aux = assignments + i;
        do
        {
            format_frequencies[aux->format]++;
            transponder_cost += formats[aux->format].cost;
            if (aux->is_split)
                aux = aux->split;
            else
                aux = NULL;
        } while (aux != NULL);
    }

    print_section(f, "Performance Parameters");
    fprintf(f, "Highest link load: %ld\\\\\n", highest_loaded_link);
    fprintf(f, "Highest used fsu: %ld out of %d\\\\\n", highest_fsu, MAX_SPECTRAL_SLOTS);
    fprintf(f, "Total used fsus: %ld\\\\\n", total_fsus);
    fprintf(f, "Modulation format use counts: {");
    for (uint64_t i = 0; i < MODULATION_FORMATS_DIM; i++)
    {
        fprintf(f, "%ld", format_frequencies[i]);
        if (i != MODULATION_FORMATS_DIM - 1)
        {
            fprintf(f, ", ");
        }
    }
    fprintf(f, "}\\\\\n");
    fprintf(f, "Transponder costs: %ld\n", transponder_cost);
    print_section(f, "Link Load Distribution");
    print_link_usage(topology_node_count, total_link_loads, f);
    print_section(f, "Path Length Distribution");
    print_path_lengths(topology_node_count, assignments, requests_count, f);
    if (protection == DEDICATED_PROTECTION)
    {
        print_section(f, "Protection Path Length Distribution");
        print_path_lengths(topology_node_count, protections, requests_count, f);
    }
    print_section(f, "Utilization Entropy");
    print_entropy(network, coordinates, topology_node_count, f, used_frequency_slots);
    print_section(f, "Shannon Entropy");
    print_shannon_entropy(network, coordinates, topology_node_count, f, used_frequency_slots);

        print_section(f, "Assignments");
    for (uint64_t i = 0; i < requests_count; i++)
    {
        print_assignment_latex(network, assignments + i, coordinates, protections + i, f);
        if ((i % 3) == 2 && i != requests_count - 1)
            print_linebreak(f);
    }

    print_epilogue(f);

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
}

void run_requests(const network_t *network, const uint64_t *connection_requests, int ascending, char *title, FILE *file, coordinate *coordinates, protection_type protection, routing_algorithms routing_a, slot_assignment_algorithms slot_a, modulation_format_assignment_algorithms modulation_a)
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

    path_t **to_free = generate_routing(network, requests, requests_count, formats, MODULATION_FORMATS_DIM, assignments, protections, protection, used_frequency_slots, routing_a, slot_a, modulation_a);

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
    free(requests);

    print_report(network, assignments, requests_count, protections, used_frequency_slots, total_link_loads, title, file, coordinates, protection);

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
    mkdir("./reports", S_IRWXU | S_IRWXG | S_IROTH);
    network_t *german_n = build_network(GERMAN_TOPOLOGY_SIZE, german_links, GERMAN_LINKS_SIZE);
    // run_requests(german_n, (uint64_t *)g7_1, 0, stdout,SHARED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    // run_requests(german_n, (uint64_t *)g7_1, 1, stdout,SHARED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    // run_requests(german_n, (uint64_t *)g7_2, 0, stdout,SHARED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    // run_requests(german_n, (uint64_t *)g7_2, 1, stdout,SHARED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    // run_requests(german_n, (uint64_t *)g7_3, 0, stdout,SHARED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    // run_requests(german_n, (uint64_t *)g7_3, 1, stdout,SHARED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    // run_requests(german_n, (uint64_t *)g7_4, 0, stdout,SHARED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    // run_requests(german_n, (uint64_t *)g7_4, 1, stdout,SHARED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    // run_requests(german_n, (uint64_t *)g7_5, 0, stdout,SHARED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    // run_requests(german_n, (uint64_t *)g7_5, 1, stdout,SHARED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    free_network(german_n);
    network_t *italian_n = build_network(ITALIAN_TOPOLOGY_SIZE, italian_links, ITALIAN_LINKS_SIZE);
    // run_requests(italian_n, (uint64_t *)IT10_1, 0, stdout, SHARED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    // run_requests(italian_n, (uint64_t *)IT10_1, 1, stdout, SHARED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    // run_requests(italian_n, (uint64_t *)IT10_2, 0, stdout, SHARED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    // run_requests(italian_n, (uint64_t *)IT10_2, 1, stdout, SHARED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    // run_requests(italian_n, (uint64_t *)IT10_3, 0, stdout, SHARED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    // run_requests(italian_n, (uint64_t *)IT10_3, 1, stdout, SHARED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    // run_requests(italian_n, (uint64_t *)IT10_4, 0, stdout, SHARED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    // run_requests(italian_n, (uint64_t *)IT10_4, 1, stdout, SHARED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    // run_requests(italian_n, (uint64_t *)IT10_5, 0, stdout, SHARED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    FILE *f = fopen("reports/out.tex", "w+");
    run_requests(italian_n, (uint64_t *)IT10_5, 1, "Italian - Shared Protection - Custom Algorithm", f, italian_coordinates, SHARED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    fclose(f);
    free_network(italian_n);
}

int main(void)
{
    run_solutions();
    return 0;
}
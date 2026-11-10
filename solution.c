#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>
//#include <sys/stat.h>
//#include <sys/types.h>

#ifdef _WIN32
    #include <direct.h>
    #define mkdir(path, mode) _mkdir(path)  
#else
    #include <sys/stat.h>
    #include <sys/types.h>
#endif

#include "network.h"
#include "problem.h"
#include "routing_solution.h"
#include "graph_drawing.h"

#define LINK_USAGE_MAX_BINS 20
#define LINK_USAGE_PLOT_INTERVAL 25

#define PATH_LENGTH_MAX_PLOT 2000
#define PATH_LENGTH_PLOT_INTERVAL 200
#define BAR_PLOT_WIDTH 12
#define BIG_BAR_PLOT_WIDTH 18

#define ENTROPY_MAX_PLOT 1.0
#define ENTROPY_PLOT_INTERVAL 0.01
#define ENTROPY_PLOT_INTERVALS_COUNT 100
#define SHANNON_ENTROPY_PLOT_INTERVAL 0.05
#define SHANNON_ENTROPY_PLOT_INTERVALS_COUNT 20

#define TOPOLOGY_X_SCALE 2.0
#define BIG_TOPOLOGY_X_SCALE 3.0
#define TOPOLOGY_Y_SCALE 2.0
#define BIG_TOPOLOGY_Y_SCALE 2.5

#define REPORT_ASSIGNMENTS 0

void free_assignment(assignment_t *assignment)
{
    if (assignment->is_split)
        free_assignment(assignment->split);
    free(assignment);
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
    edge_style edges = {0};
    print_graph(network, edges, coordinates, paths, protection->path == NULL || protection->path->length == -1 ? 1 : 2, 0.9, 0.8, file, 0);
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

//////////
double utilization_entropy(const dynamic_char_array *frequency_slots)
{ 
    if (frequency_slots->size <= 0)
        {
            return 0;
        }
    int UUSED = 1;
    int UN_USED = 0;
    uint64_t status_changes = 0;
    //int status = get_element(frequency_slots, 0) == USED ? USED : UNUSED;
    int status = get_element(frequency_slots, 0);
    if (status ==  USED || status == PROTECTION_USED){
        status = UUSED;
    }else{
        status = UN_USED;
    }

    for (uint64_t i = 0; i < frequency_slots->size; i++)
    {//go over all FSUs
        int slot_status = get_element(frequency_slots, i);
        if (slot_status == USED || slot_status == PROTECTION_USED){
            slot_status = UUSED;
        }else{
            slot_status = UN_USED;
        }

        if (slot_status != status)
        {
            status = slot_status;
            status_changes++;
        }
    }
    return (double)status_changes / (MAX_SPECTRAL_SLOTS - 1);
}

///////////
double shannon_entropy(const dynamic_char_array *frequency_slots)
{
    if (frequency_slots->size <= 0)
        return 0;

    int UUSED = 1;
    int UN_USED = 0;
    double entropy = 0;
    uint64_t status_changes = 0;
    uint64_t block_size = 0;
    //int status = get_element(frequency_slots, 0) == USED ? USED : UNUSED;

    int status = get_element(frequency_slots, 0);
    if (status ==  USED || status == PROTECTION_USED){
        status = UUSED;
    }else{
        status = UN_USED;
    }

    for (uint64_t i = 0; i < frequency_slots->size; i++)
    {//go over all FSUs
        //int slot_status = get_element(frequency_slots, i) == USED ? USED : UNUSED;

        int slot_status = get_element(frequency_slots, i);
        if (slot_status == USED || slot_status == PROTECTION_USED){
            slot_status = UUSED;
        }else{
            slot_status = UN_USED;
        }        

        if (slot_status != status)
        {
            if (slot_status == UN_USED)
                entropy += ((double)block_size / MAX_SPECTRAL_SLOTS) * log((double)block_size / MAX_SPECTRAL_SLOTS);
            status = slot_status;
            status_changes++;
            block_size = 0;
        }
        block_size++;
    }

    return -entropy;
}
//////////////

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
    snprintf(_data->buffer, sizeof(_data->buffer), "%zd", get_link_weight(_data->network, from, to));
    return _data->buffer;
}

typedef struct link_load_data_t
{
    char buffer[32];
    const uint64_t *loads;
    uint64_t node_count;
    uint64_t max_load;
} link_load_data_t;

char *read_link_load(const void *data, uint64_t from, uint64_t to)
{
    link_load_data_t *_data = (link_load_data_t *)data;
    snprintf(_data->buffer, sizeof(_data->buffer), "%" PRIu64, _data->loads[from * _data->node_count + to]);
    return _data->buffer;
}

hex_color link_load_color(const void *data, uint64_t from, uint64_t to)
{
    const link_load_data_t *_data = data;
    return color_scale(_data->max_load ? (double)_data->loads[from * _data->node_count + to] / _data->max_load : 0);
}

void print_link_load_map(const network_t *network, coordinate *coordinates, const uint64_t *total_link_loads, uint64_t max_load, FILE *f)
{
    link_load_data_t data = {.loads = total_link_loads, .node_count = network->node_count, .max_load = max_load};
    edge_style edges = {.labeler = read_link_load, .label_data = &data, .colorer = link_load_color, .color_data = &data};
    print_graph(network, edges, coordinates, NULL, 0, BIG_TOPOLOGY_X_SCALE, BIG_TOPOLOGY_Y_SCALE, f, BIDIRECTIONAL | FIT_TO_PAGE | THICK_EDGES);
    print_color_scale_legend(f, 0, max_load, "Load (Gb/s)");
}

void print_spectrum_occupancy(const network_t *network, const dynamic_char_array *used_frequency_slots, FILE *f)
{
    static const char *status_names[] = {[UNUSED] = NULL, [USED] = "Working", [PROTECTION_USED] = "Protection", [GUARD_BAND] = "Guard band", [PROTECTION_GUARD_BAND] = "Protection guard band"};
    static const hex_color status_colors[] = {[UNUSED] = {255, 255, 255}, [USED] = {66, 133, 244}, [PROTECTION_USED] = {251, 188, 5}, [GUARD_BAND] = {150, 150, 150}, [PROTECTION_GUARD_BAND] = {222, 200, 140}};
    uint64_t n = network->node_count;
    char **labels = calloc(n * n, sizeof(char *));
    const dynamic_char_array **rows = calloc(n * n, sizeof(dynamic_char_array *));
    uint64_t row_count = 0, slot_count = 1;
    for (uint64_t from = 0; from < n; from++)
    {
        for (uint64_t to = 0; to < n; to++)
        {
            if (from == to || get_link_weight(network, from, to) == -1)
                continue;
            labels[row_count] = malloc(64);
            snprintf(labels[row_count], 64, "%" PRIu64 "\\(\\to\\)%" PRIu64, from + 1, to + 1);
            rows[row_count] = used_frequency_slots + from * n + to;
            if (rows[row_count]->size > slot_count)
                slot_count = rows[row_count]->size;
            row_count++;
        }
    }
    if (slot_count > MAX_SPECTRAL_SLOTS)
        slot_count = MAX_SPECTRAL_SLOTS;
    spectrum_map map = {.row_count = row_count, .row_labels = labels, .rows = rows, .slot_count = slot_count, .status_count = sizeof(status_colors) / sizeof(status_colors[0]), .status_names = status_names, .status_colors = status_colors, .skip_status = UNUSED, .width = 15};
    print_spectrum_map(map, f);
    for (uint64_t i = 0; i < row_count; i++)
        free(labels[i]);
    free(labels);
    free(rows);
}

void print_link_usage(uint64_t topology_node_count, uint64_t *total_link_loads, FILE *f)
{
    uint64_t max_load = 0;
    for (uint64_t i = 0; i < topology_node_count * topology_node_count; i++)
        if (total_link_loads[i] > max_load)
            max_load = total_link_loads[i];
    uint64_t interval = LINK_USAGE_PLOT_INTERVAL;
    while (max_load / interval >= LINK_USAGE_MAX_BINS)
        interval *= 2;
    uint64_t bins = max_load / interval + 1;

    data_point *points = calloc(bins, sizeof(data_point));
    char **labels = calloc(bins, sizeof(char *));
    for (uint64_t i = 0; i < bins; i++)
    {
        labels[i] = calloc(32, sizeof(char));
        snprintf(labels[i], 32, "%" PRIu64 "-%" PRIu64, i * interval, (i + 1) * interval);
        points[i].x = labels[i];
    }
    for (uint64_t i = 0; i < topology_node_count * topology_node_count; i++)
        if (total_link_loads[i] > 0)
            points[total_link_loads[i] / interval].y++;

    bar_plot p = {.width = BIG_BAR_PLOT_WIDTH, .samples_count = 1, .y_label = "Links", .x_label = "Link load (Gb/s)", .data_points = points, .data_points_count = bins, .x_labels = labels, .x_labels_count = bins};
    print_plot(p, f);
    for (uint64_t i = 0; i < bins; i++)
        free(labels[i]);
    free(labels);
    free(points);
}

void print_path_lengths(uint64_t topology_node_count, assignment_t *assignments, uint64_t request_count, FILE *f)
{
    data_point points[PATH_LENGTH_MAX_PLOT / PATH_LENGTH_PLOT_INTERVAL + 1][1] = {0};
    char *link_loads_x_labels[PATH_LENGTH_MAX_PLOT / PATH_LENGTH_PLOT_INTERVAL + 1];
    uint64_t index = 0;
    for (uint64_t i = 0; i < PATH_LENGTH_MAX_PLOT; i += PATH_LENGTH_PLOT_INTERVAL)
    {
        link_loads_x_labels[index] = calloc(16, sizeof(char));
        points[index][0].x = link_loads_x_labels[index];
        snprintf(link_loads_x_labels[index], 16, "%ld-%ld", i, i + PATH_LENGTH_PLOT_INTERVAL);
        index++;
    }
    bar_plot p = {.width = BAR_PLOT_WIDTH, .samples_count = 1, .y_label = "Paths", .x_label = "Path length (km)", .data_points = (data_point *)points, .x_labels_count = (PATH_LENGTH_MAX_PLOT / PATH_LENGTH_PLOT_INTERVAL), .x_labels = link_loads_x_labels};
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
    edge_style edges = {.labeler = read_entropy, .label_data = &data};
    print_graph(network, edges, coordinates, NULL, 0, BIG_TOPOLOGY_X_SCALE, BIG_TOPOLOGY_Y_SCALE, f, BIDIRECTIONAL | FIT_TO_PAGE);
    data_point points[ENTROPY_PLOT_INTERVALS_COUNT][1] = {0};
    char *link_loads_x_labels[ENTROPY_PLOT_INTERVALS_COUNT];
    uint64_t index = 0;
    for (double i = 0; i < ENTROPY_MAX_PLOT; i += ENTROPY_PLOT_INTERVAL)
    {
        link_loads_x_labels[index] = calloc(16, sizeof(char));
        points[index][0].x = link_loads_x_labels[index];
        snprintf(link_loads_x_labels[index], 16, "%.2f-%.2f", i, i + ENTROPY_PLOT_INTERVAL);
        index++;
    }
    bar_plot p = {.width = BIG_BAR_PLOT_WIDTH, .samples_count = 1, .y_label = "Links", .x_label = "Entropy", .data_points = (data_point *)points, .x_labels_count = (ENTROPY_PLOT_INTERVALS_COUNT), .x_labels = link_loads_x_labels};
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
    edge_style edges = {.labeler = read_entropy, .label_data = &data};
    print_graph(network, edges, coordinates, NULL, 0, BIG_TOPOLOGY_X_SCALE, BIG_TOPOLOGY_Y_SCALE, f, BIDIRECTIONAL | FIT_TO_PAGE);
    data_point points[SHANNON_ENTROPY_PLOT_INTERVALS_COUNT][1] = {0};
    char *link_loads_x_labels[SHANNON_ENTROPY_PLOT_INTERVALS_COUNT];
    uint64_t index = 0;
    for (double i = 0; i < ENTROPY_MAX_PLOT; i += SHANNON_ENTROPY_PLOT_INTERVAL)
    {
        link_loads_x_labels[index] = calloc(16, sizeof(char));
        points[index][0].x = link_loads_x_labels[index];
        snprintf(link_loads_x_labels[index], 16, "%.3f-%.3f", i, i + SHANNON_ENTROPY_PLOT_INTERVAL);
        index++;
    }
    bar_plot p = {.width = BIG_BAR_PLOT_WIDTH, .samples_count = 1, .y_label = "Links", .x_label = "Entropy", .data_points = (data_point *)points, .x_labels_count = (SHANNON_ENTROPY_PLOT_INTERVALS_COUNT), .x_labels = link_loads_x_labels};
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
    edge_style edges = {.labeler = read_link_weight, .label_data = &data};
    print_graph(network, edges, coordinates, NULL, 0, TOPOLOGY_X_SCALE, TOPOLOGY_Y_SCALE, f, FIT_TO_PAGE);

    uint64_t topology_node_count = network->node_count;

    __ssize_t highest_loaded_link = -1;
    for (uint64_t i = 0; i < topology_node_count * topology_node_count; i++)
    {
        if (total_link_loads[i] > highest_loaded_link || highest_loaded_link == -1)
            highest_loaded_link = total_link_loads[i];
    }
    __ssize_t highest_fsu = -1;
    for (uint64_t i = 0; i < requests_count; i++)
    {//go over all requests
        __ssize_t slot = highest_fsu_in_assignent(assignments + i);
        if (slot > highest_fsu)
            highest_fsu = slot;
    }
    if (protection == DEDICATED_PROTECTION)///////
    {
        for(uint64_t i = 0; i < requests_count; i++)
        {
            if (protections[i].path != NULL && protections[i].path->length != -1)
            {
                __ssize_t slot = highest_fsu_in_assignent(protections + i);
                if (slot > highest_fsu)
                    highest_fsu = slot;
            }
        }
    }/////////

    //////////
    uint64_t total_fsus = 0;
    for (uint64_t i = 0; i < requests_count; i++)
    {
        assignment_t* temp = assignments + i;
        do{
            total_fsus += temp->end_slot - temp->start_slot;
            if (temp->is_split == 1)
            {
                temp = temp->split;
             
            }else {
                break;
            }

        }while(temp != NULL);

    }
    if (protection == DEDICATED_PROTECTION)
    {
        for (uint64_t i = 0; i < requests_count; i++)
        {
            if (protections[i].path != NULL && protections[i].path->length != -1)
            {
                assignment_t* temp = protections + i;
                do{
                    total_fsus += temp->end_slot - temp->start_slot;
                    if (temp->is_split == 1)
                    {
                        temp = temp->split;
                    }else{
                        break;
                    }

                }while(temp != NULL);
            }else{
                continue;
            }
    
        }
    }
    ////////////
    uint64_t format_frequencies[MODULATION_FORMATS_DIM] = {0};
    double transponder_cost = 0;
    for (uint64_t i = 0; i < requests_count; i++)
    {//go over all requests
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

    if (protection == DEDICATED_PROTECTION)
    {
        for(uint64_t i = 0; i < requests_count; i++)
        {
            if (protections[i].path != NULL && protections[i].path->length != -1)
            {
                assignment_t *aux = protections + i;
                do{
                    format_frequencies[aux->format]++;
                    transponder_cost += formats[aux->format].cost;
                    if (aux->is_split)
                        aux = aux->split;
                    else 
                        aux = NULL;

                }while(aux != NULL);
            }else{
                continue;
            }
        }
    }
    //////////////////////////////
    
    print_section(f, "Performance Parameters");
    fprintf(f, "\\begin{center}\\begin{tabular}{lr}\n\\toprule\n");
    fprintf(f, "Highest link load & %zd Gb/s \\\\\n", highest_loaded_link);
    fprintf(f, "Highest used FSU & %zd / %d \\\\\n", highest_fsu, MAX_SPECTRAL_SLOTS);
    fprintf(f, "Total used FSUs & %" PRIu64 " \\\\\n", total_fsus);
    fprintf(f, "Transponder cost & %.1f \\\\\n\\midrule\n", transponder_cost);
    for (uint64_t i = 0; i < MODULATION_FORMATS_DIM; i++)
        fprintf(f, "%s transponders & %" PRIu64 " \\\\\n", formats[i].name, format_frequencies[i]);
    fprintf(f, "\\bottomrule\n\\end{tabular}\\end{center}\n");

    print_pagebreak(f);
    print_section(f, "Link Load Map");
    print_link_load_map(network, coordinates, total_link_loads, highest_loaded_link, f);
    print_section(f, "Link Load Distribution");
    print_link_usage(topology_node_count, total_link_loads, f);
    print_pagebreak(f);
    print_section(f, "Spectrum Occupancy");
    print_spectrum_occupancy(network, used_frequency_slots, f);
    print_pagebreak(f);
    print_section(f, "Path Length Distribution");
    print_path_lengths(topology_node_count, assignments, requests_count, f);
    if (protection == DEDICATED_PROTECTION)
    {
        print_section(f, "Protection Path Length Distribution");
        print_path_lengths(topology_node_count, protections, requests_count, f);
    }
    print_pagebreak(f);
    print_section(f, "Utilization Entropy");
    print_entropy(network, coordinates, topology_node_count, f, used_frequency_slots);
    print_pagebreak(f);
    print_section(f, "Shannon Entropy");
    print_shannon_entropy(network, coordinates, topology_node_count, f, used_frequency_slots);

    if (REPORT_ASSIGNMENTS)
    {
        print_pagebreak(f);
        print_section(f, "Assignments");
        for (uint64_t i = 0; i < requests_count; i++)
        {
            print_assignment_latex(network, assignments + i, coordinates, protections + i, f);
            if ((i % 3) == 2 && i != requests_count - 1)
                print_linebreak(f);
        }
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

void run_requests(const network_t *network, const uint64_t *connection_requests, int ascending, char *title, char *file_name, coordinate *coordinates, protection_type protection, routing_algorithms routing_a, slot_assignment_algorithms slot_a, modulation_format_assignment_algorithms modulation_a)
{
    FILE *file = fopen(file_name, "w+");
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
        assignment_t *assignment = assignments + i;
        do
        {
            for (uint64_t node = 0; node < assignment->path->length; node++)
            {
                if (assignment->path->length == -1)
                    break;
                total_link_loads[assignment->path->nodes[node] * topology_node_count + assignment->path->nodes[node + 1]] += assignment->load;
            }
            if (assignment->is_split)
            {
                assignment = assignment->split;
            }
            else
                assignment = NULL;
        } while (assignment != NULL);
    }
    if (protection == DEDICATED_PROTECTION)
    {
        for (uint64_t i = 0; i < requests_count; i++)
        {
            assignment_t *assignment = protections + i;
            if (assignment == NULL || assignment->path == NULL || assignment->path->length == -1)
                continue;
            do
            {
                for (uint64_t node = 0; node < assignment->path->length; node++)
                {
                    if (assignment->path->length == -1)
                        break;
                    total_link_loads[assignment->path->nodes[node] * topology_node_count + assignment->path->nodes[node + 1]] += assignment->load;
                }
                if (assignment->is_split)
                {
                    assignment = assignment->split;
                }
                else
                    assignment = NULL;
            } while (assignment != NULL);
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
    fclose(file);
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

void italian_no_protection_custom_algorithm(network_t *italian_n)
{
    run_requests(italian_n, (uint64_t *)IT10_1, 0, "IT10 1 - No Protection - Custom Algorithm - Descending", "reports/IT10_1-NOP-DESC.tex", italian_coordinates, NO_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(italian_n, (uint64_t *)IT10_1, 1, "IT10 1 - No Protection - Custom Algorithm - Ascending", "reports/IT10_1-NOP-ASC.tex", italian_coordinates, NO_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(italian_n, (uint64_t *)IT10_2, 0, "IT10 2 - No Protection - Custom Algorithm - Descending", "reports/IT10_2-NOP-DESC.tex", italian_coordinates, NO_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(italian_n, (uint64_t *)IT10_2, 1, "IT10 2 - No Protection - Custom Algorithm - Ascending", "reports/IT10_2-NOP-ASC.tex", italian_coordinates, NO_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(italian_n, (uint64_t *)IT10_3, 0, "IT10 3 - No Protection - Custom Algorithm - Descending", "reports/IT10_3-NOP-DESC.tex", italian_coordinates, NO_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(italian_n, (uint64_t *)IT10_3, 1, "IT10 3 - No Protection - Custom Algorithm - Ascending", "reports/IT10_3-NOP-ASC.tex", italian_coordinates, NO_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(italian_n, (uint64_t *)IT10_4, 0, "IT10 4 - No Protection - Custom Algorithm - Descending", "reports/IT10_4-NOP-DESC.tex", italian_coordinates, NO_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(italian_n, (uint64_t *)IT10_4, 1, "IT10 4 - No Protection - Custom Algorithm - Ascending", "reports/IT10_4-NOP-ASC.tex", italian_coordinates, NO_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(italian_n, (uint64_t *)IT10_5, 0, "IT10 5 - No Protection - Custom Algorithm - Descending", "reports/IT10_5-NOP-DESC.tex", italian_coordinates, NO_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(italian_n, (uint64_t *)IT10_5, 1, "IT10 5 - No Protection - Custom Algorithm - Ascending", "reports/IT10_5-NOP-ASC.tex", italian_coordinates, NO_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
}

void italian_dedicated_protection(network_t *italian_n)
{
    run_requests(italian_n, (uint64_t *)IT10_1, 0, "IT10 1 - Dedicated Protection - Custom Algorithm - Descending", "reports/IT10_1-DEDP-DESC.tex", italian_coordinates, DEDICATED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(italian_n, (uint64_t *)IT10_1, 1, "IT10 1 - Dedicated Protection - Custom Algorithm - Ascending", "reports/IT10_1-DEDP-ASC.tex", italian_coordinates, DEDICATED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(italian_n, (uint64_t *)IT10_2, 0, "IT10 2 - Dedicated Protection - Custom Algorithm - Descending", "reports/IT10_2-DEDP-DESC.tex", italian_coordinates, DEDICATED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(italian_n, (uint64_t *)IT10_2, 1, "IT10 2 - Dedicated Protection - Custom Algorithm - Ascending", "reports/IT10_2-DEDP-ASC.tex", italian_coordinates, DEDICATED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(italian_n, (uint64_t *)IT10_3, 0, "IT10 3 - Dedicated Protection - Custom Algorithm - Descending", "reports/IT10_3-DEDP-DESC.tex", italian_coordinates, DEDICATED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(italian_n, (uint64_t *)IT10_3, 1, "IT10 3 - Dedicated Protection - Custom Algorithm - Ascending", "reports/IT10_3-DEDP-ASC.tex", italian_coordinates, DEDICATED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(italian_n, (uint64_t *)IT10_4, 0, "IT10 4 - Dedicated Protection - Custom Algorithm - Descending", "reports/IT10_4-DEDP-DESC.tex", italian_coordinates, DEDICATED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(italian_n, (uint64_t *)IT10_4, 1, "IT10 4 - Dedicated Protection - Custom Algorithm - Ascending", "reports/IT10_4-DEDP-ASC.tex", italian_coordinates, DEDICATED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(italian_n, (uint64_t *)IT10_5, 0, "IT10 5 - Dedicated Protection - Custom Algorithm - Descending", "reports/IT10_5-DEDP-DESC.tex", italian_coordinates, DEDICATED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(italian_n, (uint64_t *)IT10_5, 1, "IT10 5 - Dedicated Protection - Custom Algorithm - Ascending", "reports/IT10_5-DEDP-ASC.tex", italian_coordinates, DEDICATED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
}

void italian_shared_protection(network_t *italian_n)
{
    run_requests(italian_n, (uint64_t *)IT10_1, 0, "IT10 1 - Shared Protection - Custom Algorithm - Descending", "reports/IT10_1-SHP-DESC.tex", italian_coordinates, SHARED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(italian_n, (uint64_t *)IT10_1, 1, "IT10 1 - Shared Protection - Custom Algorithm - Ascending", "reports/IT10_1-SHP-ASC.tex", italian_coordinates, SHARED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(italian_n, (uint64_t *)IT10_2, 0, "IT10 2 - Shared Protection - Custom Algorithm - Descending", "reports/IT10_2-SHP-DESC.tex", italian_coordinates, SHARED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(italian_n, (uint64_t *)IT10_2, 1, "IT10 2 - Shared Protection - Custom Algorithm - Ascending", "reports/IT10_2-SHP-ASC.tex", italian_coordinates, SHARED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(italian_n, (uint64_t *)IT10_3, 0, "IT10 3 - Shared Protection - Custom Algorithm - Descending", "reports/IT10_3-SHP-DESC.tex", italian_coordinates, SHARED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(italian_n, (uint64_t *)IT10_3, 1, "IT10 3 - Shared Protection - Custom Algorithm - Ascending", "reports/IT10_3-SHP-ASC.tex", italian_coordinates, SHARED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(italian_n, (uint64_t *)IT10_4, 0, "IT10 4 - Shared Protection - Custom Algorithm - Descending", "reports/IT10_4-SHP-DESC.tex", italian_coordinates, SHARED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(italian_n, (uint64_t *)IT10_4, 1, "IT10 4 - Shared Protection - Custom Algorithm - Ascending", "reports/IT10_4-SHP-ASC.tex", italian_coordinates, SHARED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(italian_n, (uint64_t *)IT10_5, 0, "IT10 5 - Shared Protection - Custom Algorithm - Descending", "reports/IT10_5-SHP-DESC.tex", italian_coordinates, SHARED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(italian_n, (uint64_t *)IT10_5, 1, "IT10 5 - Shared Protection - Custom Algorithm - Ascending", "reports/IT10_5-SHP-ASC.tex", italian_coordinates, SHARED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
}

void italian_benchmark(network_t *italian_n)
{
    run_requests(italian_n, (uint64_t *)IT10_1, 0, "IT10 1 - No Protection - Benchmarking Algorithm - Descending", "reports/IT10_1-BENCHMARK-DESC.tex", italian_coordinates, NO_PROTECTION, FIXED_SHORTEST_PATH, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(italian_n, (uint64_t *)IT10_1, 1, "IT10 1 - No Protection - Benchmarking Algorithm - Ascending", "reports/IT10_1-BENCHMARK-ASC.tex", italian_coordinates, NO_PROTECTION, FIXED_SHORTEST_PATH, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(italian_n, (uint64_t *)IT10_2, 0, "IT10 2 - No Protection - Benchmarking Algorithm - Descending", "reports/IT10_2-BENCHMARK-DESC.tex", italian_coordinates, NO_PROTECTION, FIXED_SHORTEST_PATH, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(italian_n, (uint64_t *)IT10_2, 1, "IT10 2 - No Protection - Benchmarking Algorithm - Ascending", "reports/IT10_2-BENCHMARK-ASC.tex", italian_coordinates, NO_PROTECTION, FIXED_SHORTEST_PATH, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(italian_n, (uint64_t *)IT10_3, 0, "IT10 3 - No Protection - Benchmarking Algorithm - Descending", "reports/IT10_3-BENCHMARK-DESC.tex", italian_coordinates, NO_PROTECTION, FIXED_SHORTEST_PATH, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(italian_n, (uint64_t *)IT10_3, 1, "IT10 3 - No Protection - Benchmarking Algorithm - Ascending", "reports/IT10_3-BENCHMARK-ASC.tex", italian_coordinates, NO_PROTECTION, FIXED_SHORTEST_PATH, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(italian_n, (uint64_t *)IT10_4, 0, "IT10 4 - No Protection - Benchmarking Algorithm - Descending", "reports/IT10_4-BENCHMARK-DESC.tex", italian_coordinates, NO_PROTECTION, FIXED_SHORTEST_PATH, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(italian_n, (uint64_t *)IT10_4, 1, "IT10 4 - No Protection - Benchmarking Algorithm - Ascending", "reports/IT10_4-BENCHMARK-ASC.tex", italian_coordinates, NO_PROTECTION, FIXED_SHORTEST_PATH, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(italian_n, (uint64_t *)IT10_5, 0, "IT10 5 - No Protection - Benchmarking Algorithm - Descending", "reports/IT10_5-BENCHMARK-DESC.tex", italian_coordinates, NO_PROTECTION, FIXED_SHORTEST_PATH, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(italian_n, (uint64_t *)IT10_5, 1, "IT10 5 - No Protection - Benchmarking Algorithm - Ascending", "reports/IT10_5-BENCHMARK-ASC.tex", italian_coordinates, NO_PROTECTION, FIXED_SHORTEST_PATH, LEAST_USED_SLOT, DEFAULT_MODULATION);
}

void german_no_protection_custom_algorithm(network_t *german_n)
{
    run_requests(german_n, (uint64_t *)g7_1, 0, "g7 1 - No Protection - Custom Algorithm - Descending", "reports/g7_1-NOP-DESC.tex", german_coordinates, NO_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(german_n, (uint64_t *)g7_1, 1, "g7 1 - No Protection - Custom Algorithm - Ascending", "reports/g7_1-NOP-ASC.tex", german_coordinates, NO_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(german_n, (uint64_t *)g7_2, 0, "g7 2 - No Protection - Custom Algorithm - Descending", "reports/g7_2-NOP-DESC.tex", german_coordinates, NO_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(german_n, (uint64_t *)g7_2, 1, "g7 2 - No Protection - Custom Algorithm - Ascending", "reports/g7_2-NOP-ASC.tex", german_coordinates, NO_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(german_n, (uint64_t *)g7_3, 0, "g7 3 - No Protection - Custom Algorithm - Descending", "reports/g7_3-NOP-DESC.tex", german_coordinates, NO_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(german_n, (uint64_t *)g7_3, 1, "g7 3 - No Protection - Custom Algorithm - Ascending", "reports/g7_3-NOP-ASC.tex", german_coordinates, NO_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(german_n, (uint64_t *)g7_4, 0, "g7 4 - No Protection - Custom Algorithm - Descending", "reports/g7_4-NOP-DESC.tex", german_coordinates, NO_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(german_n, (uint64_t *)g7_4, 1, "g7 4 - No Protection - Custom Algorithm - Ascending", "reports/g7_4-NOP-ASC.tex", german_coordinates, NO_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(german_n, (uint64_t *)g7_5, 0, "g7 5 - No Protection - Custom Algorithm - Descending", "reports/g7_5-NOP-DESC.tex", german_coordinates, NO_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(german_n, (uint64_t *)g7_5, 1, "g7 5 - No Protection - Custom Algorithm - Ascending", "reports/g7_5-NOP-ASC.tex", german_coordinates, NO_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
}

void german_dedicated_protection(network_t *german_n)
{
    run_requests(german_n, (uint64_t *)g7_1, 0, "g7 1 - Dedicated Protection - Custom Algorithm - Descending", "reports/g7_1-DEDP-DESC.tex", german_coordinates, DEDICATED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(german_n, (uint64_t *)g7_1, 1, "g7 1 - Dedicated Protection - Custom Algorithm - Ascending", "reports/g7_1-DEDP-ASC.tex", german_coordinates, DEDICATED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(german_n, (uint64_t *)g7_2, 0, "g7 2 - Dedicated Protection - Custom Algorithm - Descending", "reports/g7_2-DEDP-DESC.tex", german_coordinates, DEDICATED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(german_n, (uint64_t *)g7_2, 1, "g7 2 - Dedicated Protection - Custom Algorithm - Ascending", "reports/g7_2-DEDP-ASC.tex", german_coordinates, DEDICATED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(german_n, (uint64_t *)g7_3, 0, "g7 3 - Dedicated Protection - Custom Algorithm - Descending", "reports/g7_3-DEDP-DESC.tex", german_coordinates, DEDICATED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(german_n, (uint64_t *)g7_3, 1, "g7 3 - Dedicated Protection - Custom Algorithm - Ascending", "reports/g7_3-DEDP-ASC.tex", german_coordinates, DEDICATED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(german_n, (uint64_t *)g7_4, 0, "g7 4 - Dedicated Protection - Custom Algorithm - Descending", "reports/g7_4-DEDP-DESC.tex", german_coordinates, DEDICATED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(german_n, (uint64_t *)g7_4, 1, "g7 4 - Dedicated Protection - Custom Algorithm - Ascending", "reports/g7_4-DEDP-ASC.tex", german_coordinates, DEDICATED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(german_n, (uint64_t *)g7_5, 0, "g7 5 - Dedicated Protection - Custom Algorithm - Descending", "reports/g7_5-DEDP-DESC.tex", german_coordinates, DEDICATED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(german_n, (uint64_t *)g7_5, 1, "g7 5 - Dedicated Protection - Custom Algorithm - Ascending", "reports/g7_5-DEDP-ASC.tex", german_coordinates, DEDICATED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
}

void german_shared_protection(network_t *german_n)
{
    run_requests(german_n, (uint64_t *)g7_1, 0, "g7 1 - Shared Protection - Custom Algorithm - Descending", "reports/g7_1-SHP-DESC.tex", german_coordinates, SHARED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(german_n, (uint64_t *)g7_1, 1, "g7 1 - Shared Protection - Custom Algorithm - Ascending", "reports/g7_1-SHP-ASC.tex", german_coordinates, SHARED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(german_n, (uint64_t *)g7_2, 0, "g7 2 - Shared Protection - Custom Algorithm - Descending", "reports/g7_2-SHP-DESC.tex", german_coordinates, SHARED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(german_n, (uint64_t *)g7_2, 1, "g7 2 - Shared Protection - Custom Algorithm - Ascending", "reports/g7_2-SHP-ASC.tex", german_coordinates, SHARED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(german_n, (uint64_t *)g7_3, 0, "g7 3 - Shared Protection - Custom Algorithm - Descending", "reports/g7_3-SHP-DESC.tex", german_coordinates, SHARED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(german_n, (uint64_t *)g7_3, 1, "g7 3 - Shared Protection - Custom Algorithm - Ascending", "reports/g7_3-SHP-ASC.tex", german_coordinates, SHARED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(german_n, (uint64_t *)g7_4, 0, "g7 4 - Shared Protection - Custom Algorithm - Descending", "reports/g7_4-SHP-DESC.tex", german_coordinates, SHARED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(german_n, (uint64_t *)g7_4, 1, "g7 4 - Shared Protection - Custom Algorithm - Ascending", "reports/g7_4-SHP-ASC.tex", german_coordinates, SHARED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(german_n, (uint64_t *)g7_5, 0, "g7 5 - Shared Protection - Custom Algorithm - Descending", "reports/g7_5-SHP-DESC.tex", german_coordinates, SHARED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(german_n, (uint64_t *)g7_5, 1, "g7 5 - Shared Protection - Custom Algorithm - Ascending", "reports/g7_5-SHP-ASC.tex", german_coordinates, SHARED_PROTECTION, LEAST_USED_PATH_JOINT, LEAST_USED_SLOT, DEFAULT_MODULATION);
}

void german_benchmark(network_t *german_n)
{
    run_requests(german_n, (uint64_t *)g7_1, 0, "g7 1 - No Protection - Benchmarking Algorithm - Descending", "reports/g7_1-BENCHMARK-DESC.tex", german_coordinates, NO_PROTECTION, FIXED_SHORTEST_PATH, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(german_n, (uint64_t *)g7_1, 1, "g7 1 - No Protection - Benchmarking Algorithm - Ascending", "reports/g7_1-BENCHMARK-ASC.tex", german_coordinates, NO_PROTECTION, FIXED_SHORTEST_PATH, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(german_n, (uint64_t *)g7_2, 0, "g7 2 - No Protection - Benchmarking Algorithm - Descending", "reports/g7_2-BENCHMARK-DESC.tex", german_coordinates, NO_PROTECTION, FIXED_SHORTEST_PATH, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(german_n, (uint64_t *)g7_2, 1, "g7 2 - No Protection - Benchmarking Algorithm - Ascending", "reports/g7_2-BENCHMARK-ASC.tex", german_coordinates, NO_PROTECTION, FIXED_SHORTEST_PATH, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(german_n, (uint64_t *)g7_3, 0, "g7 3 - No Protection - Benchmarking Algorithm - Descending", "reports/g7_3-BENCHMARK-DESC.tex", german_coordinates, NO_PROTECTION, FIXED_SHORTEST_PATH, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(german_n, (uint64_t *)g7_3, 1, "g7 3 - No Protection - Benchmarking Algorithm - Ascending", "reports/g7_3-BENCHMARK-ASC.tex", german_coordinates, NO_PROTECTION, FIXED_SHORTEST_PATH, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(german_n, (uint64_t *)g7_4, 0, "g7 4 - No Protection - Benchmarking Algorithm - Descending", "reports/g7_4-BENCHMARK-DESC.tex", german_coordinates, NO_PROTECTION, FIXED_SHORTEST_PATH, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(german_n, (uint64_t *)g7_4, 1, "g7 4 - No Protection - Benchmarking Algorithm - Ascending", "reports/g7_4-BENCHMARK-ASC.tex", german_coordinates, NO_PROTECTION, FIXED_SHORTEST_PATH, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(german_n, (uint64_t *)g7_5, 0, "g7 5 - No Protection - Benchmarking Algorithm - Descending", "reports/g7_5-BENCHMARK-DESC.tex", german_coordinates, NO_PROTECTION, FIXED_SHORTEST_PATH, LEAST_USED_SLOT, DEFAULT_MODULATION);
    run_requests(german_n, (uint64_t *)g7_5, 1, "g7 5 - No Protection - Benchmarking Algorithm - Ascending", "reports/g7_5-BENCHMARK-ASC.tex", german_coordinates, NO_PROTECTION, FIXED_SHORTEST_PATH, LEAST_USED_SLOT, DEFAULT_MODULATION);
}

typedef void (*scenario_runner)(network_t *network);

typedef struct scenario
{
    const char *name;
    scenario_runner german;
    scenario_runner italian;
} scenario;

static const scenario scenarios[] = {
    {"benchmark", german_benchmark, italian_benchmark},
    {"nop", german_no_protection_custom_algorithm, italian_no_protection_custom_algorithm},
    {"dedicated", german_dedicated_protection, italian_dedicated_protection},
    {"shared", german_shared_protection, italian_shared_protection},
};
#define SCENARIOS_COUNT (sizeof(scenarios) / sizeof(scenarios[0]))

void run_solutions(int run_german, int run_italian, const int *selected)
{
    mkdir("./reports", S_IRWXU | S_IRWXG | S_IROTH);
    if (run_german)
    {
        network_t *german_n = build_network(GERMAN_TOPOLOGY_SIZE, german_links, GERMAN_LINKS_SIZE);
        for (uint64_t i = 0; i < SCENARIOS_COUNT; i++)
            if (selected[i])
                scenarios[i].german(german_n);
        free_network(german_n);
    }
    if (run_italian)
    {
        network_t *italian_n = build_network(ITALIAN_TOPOLOGY_SIZE, italian_links, ITALIAN_LINKS_SIZE);
        for (uint64_t i = 0; i < SCENARIOS_COUNT; i++)
            if (selected[i])
                scenarios[i].italian(italian_n);
        free_network(italian_n);
    }
}

void print_usage(const char *program)
{
    fprintf(stderr, "usage: %s [g7] [it10] [benchmark] [nop] [dedicated] [shared]\n", program);
    fprintf(stderr, "with no network or scenario given, all of them are run\n");
}

int main(int argc, char **argv)
{
    int run_german = 0, run_italian = 0, any_scenario = 0;
    int selected[SCENARIOS_COUNT] = {0};
    for (int i = 1; i < argc; i++)
    {
        int matched = 0;
        if (strcmp(argv[i], "g7") == 0)
            run_german = matched = 1;
        else if (strcmp(argv[i], "it10") == 0)
            run_italian = matched = 1;
        for (uint64_t j = 0; j < SCENARIOS_COUNT && !matched; j++)
        {
            if (strcmp(argv[i], scenarios[j].name) == 0)
                selected[j] = any_scenario = matched = 1;
        }
        if (!matched)
        {
            print_usage(argv[0]);
            return 1;
        }
    }
    if (!run_german && !run_italian)
        run_german = run_italian = 1;
    if (!any_scenario)
        for (uint64_t i = 0; i < SCENARIOS_COUNT; i++)
            selected[i] = 1;
    run_solutions(run_german, run_italian, selected);
    return 0;
}

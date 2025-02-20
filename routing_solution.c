
#include "routing_solution.h"
#include <stdlib.h>
#include <stdio.h>

#define K 5

typedef __ssize_t (*new_assigner)(const network_t *network, assignment_t *current_assignments, connection_request request, uint64_t request_index, uint64_t request_count, const modulation_format *formats, uint64_t formats_dim, void **data, dynamic_char_array *link_slot_usages_ret);

path_t *find_shortest_path(const network_t *network, uint64_t from_node_id, uint64_t to_node_id)
{
    path_t *const *distances = weighted_distances(network, from_node_id);
    path_t *assigned_path = distances[to_node_id];
    return assigned_path;
}

void accumulate_slots(const network_t *network, const path_t *path, dynamic_char_array *source_arrays, dynamic_char_array *array)
{
    // go through each link in the path assigned, set to 1 any slot that is occupied in any link along that path, as well as 1 FSU as guard band
    for (uint64_t i = 0; i < path->length; i++)
    {
        for (uint64_t j = 0; j < source_arrays[path->nodes[i] * network->node_count + path->nodes[i + 1]].size; j++)
        {
            if (get_element(source_arrays + path->nodes[i] * network->node_count + path->nodes[i + 1], j) != UNUSED)
            {
                set_element(array, j, USED);
            }
        }
    }
}

void set_slots_on_path(const network_t *network, const path_t *path, dynamic_char_array *arrays, uint64_t start_slot, uint64_t end_slot)
{
    for (uint64_t i = 0; i < path->length; i++)
    {
        for (uint64_t j = start_slot; j < end_slot; j++)
        {
            set_element(arrays + path->nodes[i] * network->node_count + path->nodes[i + 1], j, USED);
        }
        set_element(arrays + path->nodes[i] * network->node_count + path->nodes[i + 1], end_slot, GUARD_BAND);
    }
}

void set_slot_usages(const path_t *path, uint64_t start_slot, uint64_t end_slot, uint64_t usages[MAX_SPECTRAL_SLOTS])
{
    for (uint64_t i = start_slot; i < end_slot; i++)
    {
        usages[i] += path->length;
    }
}

__ssize_t first_fit_slot_assignment(const network_t *network, const modulation_format *formats, assignment_t *assignment_ret, dynamic_char_array *link_slot_usages_ret)
{
    dynamic_char_array *available_slots = new_dynamic_char_array();
    accumulate_slots(network, assignment_ret->path, link_slot_usages_ret, available_slots);

    uint64_t slot_requirement = (uint64_t)(formats[assignment_ret->format].channel_bandwidth / FSU_BANDWIDTH + 0.5);
    uint64_t consecutive_slots = 0;
    for (uint64_t i = 0; i < MAX_SPECTRAL_SLOTS; i++)
    {
        if (get_element(available_slots, i) == UNUSED)
        {
            consecutive_slots++;
            if (consecutive_slots >= slot_requirement)
            {
                assignment_ret->start_slot = i - slot_requirement + 1;
                assignment_ret->end_slot = i + 1;
                set_slots_on_path(network, assignment_ret->path, link_slot_usages_ret, assignment_ret->start_slot, assignment_ret->end_slot);
                free_dynamic_char_array(available_slots);
                return i;
            }
        }
        else
        {
            consecutive_slots = 0;
        }
    }
    free_dynamic_char_array(available_slots);
    return -1;
}

uint64_t sum_range(const uint64_t *array, uint64_t start_range, uint64_t end_range)
{
    uint64_t sum = 0;
    for (uint64_t i = start_range; i < end_range; i++)
        sum += array[i];
    return sum;
}

__ssize_t least_used_slot_assignment(const network_t *network, const modulation_format *formats, assignment_t *assignment_ret, dynamic_char_array *link_slot_usages_ret, uint64_t usages[MAX_SPECTRAL_SLOTS])
{
    dynamic_char_array *available_slots = new_dynamic_char_array();
    accumulate_slots(network, assignment_ret->path, link_slot_usages_ret, available_slots);

    __ssize_t least_used_usage = -1, least_used_start_slot = -1;
    uint64_t slot_requirement = (uint64_t)(formats[assignment_ret->format].channel_bandwidth / FSU_BANDWIDTH + 0.5);
    uint64_t consecutive_slots = 0;
    for (uint64_t i = 0; i < MAX_SPECTRAL_SLOTS; i++)
    {
        if (get_element(available_slots, i) == UNUSED)
        {
            consecutive_slots++;
            uint64_t usage;
            if (consecutive_slots >= slot_requirement && ((usage = sum_range(usages, i - slot_requirement + 1, i + 1)) < least_used_usage || least_used_usage == -1))
            {
                least_used_usage = usage;
                least_used_start_slot = i - slot_requirement + 1;
            }
        }
        else
        {
            consecutive_slots = 0;
        }
    }
    free_dynamic_char_array(available_slots);
    if (least_used_start_slot == -1)
        return -1;

    assignment_ret->start_slot = least_used_start_slot;
    assignment_ret->end_slot = least_used_start_slot + slot_requirement;
    set_slots_on_path(network, assignment_ret->path, link_slot_usages_ret, assignment_ret->start_slot, assignment_ret->end_slot);
    set_slot_usages(assignment_ret->path, assignment_ret->start_slot, assignment_ret->end_slot, usages);
    return least_used_start_slot;
}

__ssize_t assign_modulation_format(const modulation_format *formats, uint64_t formats_dim, assignment_t *assignment_ret)
{
    __ssize_t format_index = -1;
    for (uint64_t i = 0; i < formats_dim; i++)
    {
        if (formats[i].maximum_length >= assignment_ret->path->distance)
        {
            if (formats[i].line_rate > assignment_ret->load)
            {
                assignment_ret->format = i;
                return 0;
            }
            format_index = i;
        }
    }
    if (format_index == -1)
        return -1;
    uint64_t load = assignment_ret->load;
    assignment_ret->load = formats[format_index].line_rate;
    return load - formats[format_index].line_rate;
}

uint64_t most_loaded_link(const network_t *network, const path_t *path, uint64_t *loads)
{
    uint64_t max_load = 0;
    for (uint64_t i = 0; i < path->length; i++)
    {
        if (loads[path->nodes[i] * network->node_count + path->nodes[i + 1]] > max_load)
        {
            max_load = loads[path->nodes[i] * network->node_count + path->nodes[i + 1]];
        }
    }
    return max_load;
}

path_t *find_least_maximally_loaded_path_modified(const network_t *network, uint64_t from_node_id, uint64_t to_node_id, uint64_t *loads)
{
    path_t *const *k_paths = k_shortest_paths(network, from_node_id, to_node_id, K);
    uint64_t best_path_index = 0;
    double best_path_max_load_ratio = -1;
    for (uint64_t i = 0; i < K; i++)
    {
        double max_path_load_ratio = most_loaded_link(network, k_paths[i], loads) + k_paths[i]->length * LENGTH_LOAD_PONDERATION;
        if (max_path_load_ratio < best_path_max_load_ratio || best_path_max_load_ratio == -1)
        {
            best_path_max_load_ratio = max_path_load_ratio;
            best_path_index = i;
        }
    }
    return k_paths[best_path_index];
}

path_t *find_least_maximally_loaded_path(const network_t *network, uint64_t from_node_id, uint64_t to_node_id, uint64_t *loads)
{
    path_t *const *k_paths = k_shortest_paths(network, from_node_id, to_node_id, K);
    uint64_t best_path_index = 0;
    __ssize_t best_path_max_load = -1;
    for (uint64_t i = 0; i < K; i++)
    {
        uint64_t max_path_load = most_loaded_link(network, k_paths[i], loads);
        if (max_path_load < best_path_max_load || best_path_max_load == -1)
        {
            best_path_max_load = max_path_load;
            best_path_index = i;
        }
    }
    return k_paths[best_path_index];
}

typedef struct usages_and_loads
{
    uint64_t *usages;
    uint64_t *loads;
} usages_and_loads;

__ssize_t least_used_path(const network_t *network, assignment_t *current_assignments, connection_request request, uint64_t request_index, uint64_t request_count, const modulation_format *formats, uint64_t formats_dim, void **data, dynamic_char_array *link_slot_usages_ret)
{
    usages_and_loads *_data = (usages_and_loads *)(*data);
    if (request_index >= request_count)
    {
        if (*data != NULL)
        {
            free(_data->loads);
            free(_data->usages);
            free(*data);
        }
        return -1;
    }
    if (*data == NULL)
    {
        *data = calloc(1, sizeof(usages_and_loads));
        _data = (usages_and_loads *)*data;
        _data->loads = calloc(MAX_SPECTRAL_SLOTS, sizeof(uint64_t));
        _data->usages = calloc(network->node_count * network->node_count, sizeof(uint64_t));
    }

    __ssize_t leftover_load = request.load;
    assignment_t *assignment = current_assignments + request_index;
    path_t *assigned_path = find_least_maximally_loaded_path_modified(network, request.from_node_id, request.to_node_id, _data->usages); // Split loads over a single path
    do
    {
        assignment->load = leftover_load;
        assignment->path = assigned_path;
        assignment->is_split = 0;
        for (uint64_t i = 0; i < assignment->path->length; i++)
        {
            _data->usages[assignment->path->nodes[i] * network->node_count + assignment->path->nodes[i + 1]] += assignment->load;
        }

        if (assigned_path->length == -1)
        {
            printf("\n\nFAILED TO ASSIGN A PATH\n\n");
        }

        leftover_load = assign_modulation_format(formats, formats_dim, assignment);

        if (leftover_load == -1)
        {
            printf("\n\nFAILED TO ASSIGN A PATH\n\n");
        }

        // first_fit_slot_assignment(network, formats, assignment, link_slot_usages_ret);
        least_used_slot_assignment(network, formats, assignment, link_slot_usages_ret, _data->loads);

        if (leftover_load)
        {
            assignment_t *split_assignment = calloc(1, sizeof(assignment_t));
            assignment->split = split_assignment;
            assignment->is_split = 1;
            split_assignment->is_split = 0;

            assignment = split_assignment;
        }
    } while (leftover_load);

    return request_index + 1;
}

__ssize_t fixed_shortest_path(const network_t *network, assignment_t *current_assignments, connection_request request, uint64_t request_index, uint64_t request_count, const modulation_format *formats, uint64_t formats_dim, void **data, dynamic_char_array *link_slot_usages_ret)
{

    if (request_index >= request_count)
    {
        if (*data != NULL)
            free(*data);
        return -1;
    }
    if (*data == NULL)
    {
        *data = calloc(MAX_SPECTRAL_SLOTS, sizeof(uint64_t));
    }

    uint64_t *_data = (uint64_t *)(*data);

    __ssize_t leftover_load = request.load;
    assignment_t *assignment = current_assignments + request_index;
    path_t *assigned_path = find_shortest_path(network, request.from_node_id, request.to_node_id); // Split loads over a single path
    do
    {
        assignment->load = leftover_load;
        assignment->path = assigned_path;
        assignment->is_split = 0;

        if (assigned_path->length == -1)
        {
            printf("\n\nFAILED TO ASSIGN A PATH\n\n");
        }

        leftover_load = assign_modulation_format(formats, formats_dim, assignment);

        if (leftover_load == -1)
        {
            printf("\n\nFAILED TO ASSIGN A PATH\n\n");
        }

        // first_fit_slot_assignment(network, formats, assignment, link_slot_usages_ret);
        least_used_slot_assignment(network, formats, assignment, link_slot_usages_ret, _data);

        if (leftover_load)
        {
            assignment_t *split_assignment = calloc(1, sizeof(assignment_t));
            assignment->split = split_assignment;
            assignment->is_split = 1;
            split_assignment->is_split = 0;

            assignment = split_assignment;
        }
    } while (leftover_load);

    return request_index + 1;
}

void run_routing_algorithm(const network_t *network, connection_request *requests, uint64_t requests_dim, const modulation_format *formats, uint64_t formats_dim, new_assigner algorithm, assignment_t *assignments_ret, dynamic_char_array *link_slot_usages_ret)
{
    void *data = NULL;
    uint64_t index = 0;
    while (index != -1)
    {
        index = algorithm(network, assignments_ret, requests[index], index, requests_dim, formats, formats_dim, &data, link_slot_usages_ret);
    }
    return;
}

void generate_routing(const network_t *network, connection_request *requests, uint64_t requests_dim, const modulation_format *formats, uint64_t formats_dim, assignment_t *assignments_ret, dynamic_char_array *link_slot_usages_ret, routing_algorithms algorithm)
{
    new_assigner routing_algorithm = algorithm == FIXED_SHORTEST_PATH ? fixed_shortest_path : least_used_path;
    run_routing_algorithm(network, requests, requests_dim, formats, formats_dim, routing_algorithm, assignments_ret, link_slot_usages_ret);
}

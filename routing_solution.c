
#include "routing_solution.h"
#include <stdlib.h>
#include <stdio.h>

#define K 5

path_t *find_shortest_path(const network_t *network, uint64_t from_node_id, uint64_t to_node_id)
{
    path_t *const *distances = weighted_distances(network, from_node_id);
    path_t *assigned_path = distances[to_node_id];
    return assigned_path;
}

path_t *find_shortest_path_wrapper(const network_t *network, uint64_t from_node_id, uint64_t to_node_id, uint64_t *)
{
    return find_shortest_path(network, from_node_id, to_node_id);
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

void set_slots_on_path(const network_t *network, const path_t *path, dynamic_char_array *arrays, uint64_t start_slot, uint64_t end_slot, char value, char guard_band_value)
{
    for (uint64_t i = 0; i < path->length; i++)
    {
        for (uint64_t j = start_slot; j < end_slot; j++)
        {
            set_element(arrays + path->nodes[i] * network->node_count + path->nodes[i + 1], j, value);
        }
        set_element(arrays + path->nodes[i] * network->node_count + path->nodes[i + 1], end_slot, guard_band_value);
    }
}

void set_slot_usages(const path_t *path, uint64_t start_slot, uint64_t end_slot, uint64_t usages[MAX_SPECTRAL_SLOTS])
{
    for (uint64_t i = start_slot; i < end_slot; i++)
    {
        usages[i] += path->length;
    }
}

__ssize_t first_fit_slot_assignment(const network_t *network, const modulation_format *formats, assignment_t *assignment_ret, dynamic_char_array *link_slot_usages_ret, protection_type protection)
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
                set_slots_on_path(network, assignment_ret->path, link_slot_usages_ret, assignment_ret->start_slot, assignment_ret->end_slot, protection == NO_PROTECTION ? USED : PROTECTION_USED, protection == NO_PROTECTION ? GUARD_BAND : PROTECTION_GUARD_BAND);
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

__ssize_t least_used_slot_assignment(const network_t *network, const modulation_format *formats, assignment_t *assignment_ret, dynamic_char_array *link_slot_usages_ret, uint64_t usages[MAX_SPECTRAL_SLOTS], protection_type protection)
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
    set_slots_on_path(network, assignment_ret->path, link_slot_usages_ret, assignment_ret->start_slot, assignment_ret->end_slot, protection == NO_PROTECTION ? USED : PROTECTION_USED, protection == NO_PROTECTION ? GUARD_BAND : PROTECTION_GUARD_BAND);
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

path_t *find_least_maximally_loaded_path_modified_validators(const network_t *network, uint64_t from_node_id, uint64_t to_node_id, uint64_t *loads, link_validator validator, const void *data)
{
    path_t **k_paths = modified_k_shortest_paths(network, from_node_id, to_node_id, K, validator, data);
    if (k_paths[0] == NULL || k_paths[0]->distance == -1)
    {
        path_t *to_ret = k_paths[0];
        free(k_paths);
        return to_ret;
    }
    uint64_t best_path_index = 0;
    double best_path_max_load_ratio = -1;
    for (uint64_t i = 0; i < K && k_paths[i] != NULL; i++)
    {
        double max_path_load_ratio = most_loaded_link(network, k_paths[i], loads) + k_paths[i]->length * LENGTH_LOAD_PONDERATION;
        if (max_path_load_ratio < best_path_max_load_ratio || best_path_max_load_ratio == -1)
        {
            best_path_max_load_ratio = max_path_load_ratio;
            best_path_index = i;
        }
    }
    for (uint64_t i = 0; i < K && k_paths[i] != NULL; i++)
    {
        if (i == best_path_index)
            continue;
        free(k_paths[i]->nodes);
        free(k_paths[i]);
    }
    path_t *to_ret = k_paths[best_path_index];
    free(k_paths);
    return to_ret;
}

path_t *find_least_maximally_loaded_path_modified(const network_t *network, uint64_t from_node_id, uint64_t to_node_id, uint64_t *loads)
{
    path_t *const *k_paths = k_shortest_paths(network, from_node_id, to_node_id, K);
    uint64_t best_path_index = 0;
    double best_path_max_load_ratio = -1;
    for (uint64_t i = 0; i < K && k_paths[i] != NULL; i++)
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

typedef __ssize_t (*modulator_algorith)(const modulation_format *formats, uint64_t formats_dim, assignment_t *assignment_ret);

typedef path_t **(*routing_assigner)(const network_t *network, connection_request *requests, uint64_t request_count, assignment_t *assignments_ret, assignment_t *protections, protection_type protection);
typedef void (*modulation_assigner)(const network_t *network, connection_request *requests, uint64_t request_count, assignment_t *assignments_ret, assignment_t *protections_ret, protection_type protection, const modulation_format *formats, uint64_t formats_dim);
typedef void (*slot_assigner)(const network_t *network, const modulation_format *formats, assignment_t *assignments_ret, assignment_t *protections, protection_type protection, uint64_t request_count, dynamic_char_array *link_slot_usages_ret);

void run_modulator(const network_t *network, connection_request *requests, uint64_t request_count, assignment_t *assignments_ret, const modulation_format *formats, uint64_t formats_dim, modulator_algorith modulator)
{
    for (uint64_t request_index = 0; request_index < request_count; request_index++)
    {
        connection_request request = requests[request_index];

        __ssize_t leftover_load = request.load;
        assignment_t *assignment = assignments_ret + request_index;
        if (assignment == NULL || assignment->path == NULL || assignment->path->distance == -1)
            continue;
        do
        {
            assignment->load = leftover_load;
            assignment->is_split = 0;

            leftover_load = modulator(formats, formats_dim, assignment);

            if (leftover_load == -1)
            {
                printf("\n\nFAILED TO ASSIGN A MODULATION FORMAT\n\n");
            }

            if (leftover_load)
            {
                assignment_t *split_assignment = calloc(1, sizeof(assignment_t));
                assignment->split = split_assignment;
                assignment->is_split = 1;
                split_assignment->path = assignment->path;
                split_assignment->is_split = 0;

                assignment = split_assignment;
            }
        } while (leftover_load);
    }
}

void run_default_modulator(const network_t *network, connection_request *requests, uint64_t request_count, assignment_t *assignments_ret, assignment_t *protections_ret, protection_type protection, const modulation_format *formats, uint64_t formats_dim)
{
    run_modulator(network, requests, request_count, assignments_ret, formats, formats_dim, assign_modulation_format);
    if (protection == DEDICATED_PROTECTION || protection == SHARED_PROTECTION)
        run_modulator(network, requests, request_count, protections_ret, formats, formats_dim, assign_modulation_format);
}

void run_LUS_slotting_internal(const network_t *network, const modulation_format *formats, assignment_t *assignments_ret, assignment_t *protections, protection_type protection, uint64_t request_count, dynamic_char_array *link_slot_usages_ret, uint64_t *usages)
{
    if (protection == DEDICATED_PROTECTION)
        assignments_ret = protections;
    for (uint64_t request_index = 0; request_index < request_count; request_index++)
    {

        assignment_t *assignment = assignments_ret + request_index;
        if (assignment == NULL || assignment->path == NULL || assignment->path->distance == -1)
            continue;
        while (assignment != NULL)
        {

            least_used_slot_assignment(network, formats, assignment, link_slot_usages_ret, usages, protection);

            if (assignment->is_split)
            {
                assignment = assignment->split;
            }
            else
            {
                assignment = NULL;
            }
        }
    }
}

void run_LUS_slotting(const network_t *network, const modulation_format *formats, assignment_t *assignments_ret, assignment_t *protections, protection_type protection, uint64_t request_count, dynamic_char_array *link_slot_usages_ret)
{
    uint64_t *usages = calloc(MAX_SPECTRAL_SLOTS, sizeof(uint64_t));

    run_LUS_slotting_internal(network, formats, assignments_ret, protections, NO_PROTECTION, request_count, link_slot_usages_ret, usages);

    if (protection != NO_PROTECTION)
    {
        run_LUS_slotting_internal(network, formats, assignments_ret, protections, protection, request_count, link_slot_usages_ret, usages);
    }

    free(usages);
}

void run_FFS_slotting_internal(const network_t *network, const modulation_format *formats, assignment_t *assignments_ret, assignment_t *protections, protection_type protection, uint64_t request_count, dynamic_char_array *link_slot_usages_ret)
{
    if (protection == DEDICATED_PROTECTION)
        assignments_ret = protections;
    for (uint64_t request_index = 0; request_index < request_count; request_index++)
    {

        assignment_t *assignment = assignments_ret + request_index;
        if (assignment == NULL || assignment->path == NULL || assignment->path->distance == -1)
            continue;
        while (assignment != NULL)
        {

            first_fit_slot_assignment(network, formats, assignment, link_slot_usages_ret, protection);

            if (assignment->is_split)
            {
                assignment = assignment->split;
            }
            else
            {
                assignment = NULL;
            }
        }
    }
}

void run_FFS_slotting(const network_t *network, const modulation_format *formats, assignment_t *assignments_ret, assignment_t *protections, protection_type protection, uint64_t request_count, dynamic_char_array *link_slot_usages_ret)
{
    run_FFS_slotting_internal(network, formats, assignments_ret, protections, NO_PROTECTION, request_count, link_slot_usages_ret);
    if (protection != NO_PROTECTION)
    {
        run_FFS_slotting_internal(network, formats, assignments_ret, protections, protection, request_count, link_slot_usages_ret);
    }
}

path_t **run_SPF_routing(const network_t *network, connection_request *requests, uint64_t request_count, assignment_t *assignments_ret, assignment_t *protections, protection_type protection)
{
    for (uint64_t request_index = 0; request_index < request_count; request_index++)
    {
        connection_request request = requests[request_index];

        assignment_t *assignment = assignments_ret + request_index;
        path_t *assigned_path = find_shortest_path(network, request.from_node_id, request.to_node_id); // Split loads over a single path
        assignment->path = assigned_path;
        if (assigned_path->length == -1)
        {
            printf("\n\nFAILED TO ASSIGN A PATH\n\n");
        }
    }
    return NULL;
}

path_t **run_LML_routing(const network_t *network, connection_request *requests, uint64_t request_count, assignment_t *assignments_ret, assignment_t *protections, protection_type protection)
{
    uint64_t *loads = calloc(network->node_count * network->node_count, sizeof(uint64_t));
    for (uint64_t request_index = 0; request_index < request_count; request_index++)
    {
        connection_request request = requests[request_index];

        assignment_t *assignment = assignments_ret + request_index;
        path_t *assigned_path = find_least_maximally_loaded_path_modified(network, request.from_node_id, request.to_node_id, loads); // Split loads over a single path
        assignment->path = assigned_path;
        if (assigned_path->length == -1)
        {
            printf("\n\nFAILED TO ASSIGN A PATH\n\n");
        }
        else
        {
            for (uint64_t i = 0; i < assignment->path->length; i++)
            {
                loads[assignment->path->nodes[i] * network->node_count + assignment->path->nodes[i + 1]] += request.load;
            }
        }
    }
    free(loads);
    return NULL;
}

void highest_load(const network_t *network, uint64_t *loads, uint64_t *from_node_ret, uint64_t *to_node_ret)
{
    uint64_t max_load = 0;
    for (uint64_t i = 0; i < network->node_count; i++)
    {
        for (uint64_t j = 0; j < network->node_count; j++)
        {
            if (loads[network->node_count * i + j] > max_load)
            {
                max_load = loads[network->node_count * i + j];
                *from_node_ret = i;
                *to_node_ret = j;
            }
        }
    }
}

int contains_link(const network_t *network, uint64_t from, uint64_t to, const path_t *path)
{
    for (uint64_t i = 0; i < path->length; i++)
    {
        if (path->nodes[i] == from && path->nodes[i + 1] == to)
            return 1;
    }
    return 0;
}

void change_path(const network_t *network, assignment_t *assignment, uint64_t *loads, uint64_t path_load, const path_t *new_path)
{
    for (uint64_t i = 0; i < assignment->path->length; i++)
    {
        loads[assignment->path->nodes[i] * network->node_count + assignment->path->nodes[i + 1]] -= path_load;
    }

    assignment->path = new_path;
    // print_path(assignment->path, stdout);
    for (uint64_t i = 0; i < assignment->path->length; i++)
    {
        loads[assignment->path->nodes[i] * network->node_count + assignment->path->nodes[i + 1]] += path_load;
    }
}

uint64_t assignment_load(const assignment_t *assignment)
{
    if (assignment->is_split)
    {
        return assignment->load + assignment_load(assignment->split);
    }
    return assignment->load;
}

// obviously, don't consider source and sink nodes
int is_node_disjoint(const network_t *network, uint64_t from, uint64_t to, const void *data)
{
    const path_t *_data = (const path_t *)(data);
    for (uint64_t i = 0; i < _data->length; i++)
    {
        if (to == _data->nodes[i])
            return 0;
    }
    return 1;
}

path_t **run_LML_routing_modified_internal(const network_t *network, connection_request *requests, uint64_t request_count, assignment_t *assignments_ret, assignment_t *protections, protection_type protection, uint64_t *loads)
{
    assignment_t *to_assign = protection == NO_PROTECTION ? assignments_ret : protections;
    path_t **to_ret = protection == NO_PROTECTION ? NULL : calloc(request_count + 1, sizeof(path_t *));
    uint64_t to_ret_index = 0;
    for (uint64_t request_index = 0; request_index < request_count; request_index++)
    {
        connection_request request = requests[request_index];

        assignment_t *assignment = to_assign + request_index;
        path_t *assigned_path;
        if (protection == DEDICATED_PROTECTION)
        {
            assigned_path = find_least_maximally_loaded_path_modified_validators(network, request.from_node_id, request.to_node_id, loads, is_node_disjoint, assignments_ret[request_index].path); // Split loads over a single path
            to_ret[to_ret_index++] = assigned_path;
        }
        else
        {
            assigned_path = find_least_maximally_loaded_path_modified(network, request.from_node_id, request.to_node_id, loads); // Split loads over a single path
        }
        assignment->path = assigned_path;
        if (assigned_path->length == -1)
        {
            printf("\n\nFAILED TO ASSIGN A PATH\n\n");
        }
        else
        {
            for (uint64_t i = 0; i < assignment->path->length; i++)
            {
                loads[assignment->path->nodes[i] * network->node_count + assignment->path->nodes[i + 1]] += request.load;
            }
        }
    }

    if (protection == NO_PROTECTION)
    {
        uint64_t could_reduce_load;
        do
        {
            could_reduce_load = 0;
            uint64_t from, to;
            highest_load(network, loads, &from, &to);
            uint64_t highest_load_value = loads[from * network->node_count + to];
            __ssize_t best_candidate_index = -1;
            const path_t *best_candidate_alternative_path = NULL;
            __ssize_t max_reduction = 0;
            for (uint64_t i = 0; i < request_count; i++)
            {
                if (contains_link(network, from, to, to_assign[i].path))
                {
                    uint64_t candidate_load = requests[i].load;
                    const path_t *alternative_path = find_least_maximally_loaded_path_modified(network, to_assign[i].path->nodes[0], to_assign[i].path->nodes[to_assign[i].path->length], loads); // Split loads over a single path

                    uint64_t alternative_path_maximal_load = most_loaded_link(network, alternative_path, loads);
                    __ssize_t reduction = +highest_load_value - alternative_path_maximal_load - candidate_load;
                    if (reduction > max_reduction)
                    {
                        max_reduction = reduction;
                        best_candidate_index = i;
                        best_candidate_alternative_path = alternative_path;
                    }
                }
            }
            if (best_candidate_index != -1)
            {
                change_path(network, to_assign + best_candidate_index, loads, requests[best_candidate_index].load, best_candidate_alternative_path);
                could_reduce_load = 1;
            }
        } while (could_reduce_load);
    }
    return to_ret;
}

path_t **run_LML_routing_modified(const network_t *network, connection_request *requests, uint64_t request_count, assignment_t *assignments_ret, assignment_t *protections, protection_type protection)
{
    uint64_t *loads = calloc(network->node_count * network->node_count, sizeof(uint64_t));
    run_LML_routing_modified_internal(network, requests, request_count, assignments_ret, protections, NO_PROTECTION, loads);

    path_t **to_ret = NULL;
    if (protection == DEDICATED_PROTECTION || protection == SHARED_PROTECTION)
    {
        to_ret = run_LML_routing_modified_internal(network, requests, request_count, assignments_ret, protections, protection, loads);
    }
    free(loads);
    return to_ret;
}

path_t **run_assignment(const network_t *network, assignment_t *assignments_ret, assignment_t *protections_ret, protection_type protection, connection_request *requests, uint64_t request_count, const modulation_format *formats, uint64_t formats_dim, dynamic_char_array *link_slot_usages_ret, routing_assigner router, slot_assigner slotter, modulation_assigner modulator)
{
    path_t **to_ret = router(network, requests, request_count, assignments_ret, protections_ret, protection);
    modulator(network, requests, request_count, assignments_ret, protections_ret, protection, formats, formats_dim);
    slotter(network, formats, assignments_ret, protections_ret, protection, request_count, link_slot_usages_ret);
    return to_ret;
}

path_t **generate_routing(const network_t *network, connection_request *requests, uint64_t requests_dim, const modulation_format *formats, uint64_t formats_dim, assignment_t *assignments_ret, assignment_t *protections_ret, protection_type protection, dynamic_char_array *link_slot_usages_ret, routing_algorithms routing_algorithm, slot_assignment_algorithms slot_assigner_algorithm, modulation_format_assignment_algorithms format_assigner)
{
    routing_assigner router = routing_algorithm == FIXED_SHORTEST_PATH ? run_SPF_routing : run_LML_routing_modified;
    switch (routing_algorithm)
    {
    case LEAST_USED_PATH:
        router = run_LML_routing;
        break;
    case LEAST_USED_PATH_JOINT:
        router = run_LML_routing_modified;
        break;
    case FIXED_SHORTEST_PATH:
    default:
        router = run_SPF_routing;
        break;
    }
    slot_assigner slot_algorithm = slot_assigner_algorithm == FIRST_FIT_SLOT ? run_FFS_slotting : run_LUS_slotting;
    modulation_assigner modulator = run_default_modulator;
    return run_assignment(network, assignments_ret, protections_ret, protection, requests, requests_dim, formats, formats_dim, link_slot_usages_ret, router, slot_algorithm, modulator);
}

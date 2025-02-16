#include "modulation_assignment.h"
#include <stdlib.h>

#define BLOCK 4
#define IS_BIT_SET(x, i) (((x)[(i) >> 3] & (1 << ((i) & 7))) != 0)
#define SET_BIT(x, i) (x)[(i) >> 3] |= (1 << ((i) & 7))
#define CLEAR_BIT(x, i) (x)[(i) >> 3] &= (1 << ((i) & 7)) ^ 0xFF

typedef struct array
{
    uint64_t *elements;
    uint64_t size;
} array;

void add_element(array *a, uint64_t element)
{
    if ((a->size % BLOCK) == 0)
    {
        a->elements = realloc(a->elements, (a->size + BLOCK) * sizeof(uint64_t));
    }
    a->elements[a->size++] = element;
}

void naive_solve_modulation_assignment(array loads, const modulation_format *formats, uint64_t formats_dim, uint64_t *assigned_formats_ret)
{
    for (uint64_t i = 0; i < formats_dim; i++)
        assigned_formats_ret[i] = 0;
    array *load_availabilities = calloc(1, sizeof(array));
    for (uint64_t load_index = 0; load_index < loads.size; load_index++)
    {
        uint64_t load = loads.elements[load_index];
        int found_flag = 0;
        for (uint64_t i = 0; i < load_availabilities->size && !found_flag; i++)
        {
            if (load < load_availabilities->elements[i])
            {
                load_availabilities->elements[i] -= load;
                found_flag = 1;
            }
        }
        int i = 0; // always use the first format, 'upgrading' paths will be addressed later
        if (formats[1].line_rate >= load || i == formats_dim - 1)
        {
            assigned_formats_ret[i]++;
            add_element(load_availabilities, load > formats[i].line_rate ? 0 : formats[i].line_rate - load);
            load -= formats[i].line_rate;
            if (load < 0)
                found_flag = 1;
        }
    }
    free(load_availabilities->elements);
    free(load_availabilities);
}

void solve_modulation_assignment(array loads, const modulation_format *formats, uint64_t formats_dim, uint64_t *assigned_formats_ret)
{
    for (uint64_t i = 0; i < formats_dim; i++)
        assigned_formats_ret[i] = 0;
    array *load_availabilities = calloc(1, sizeof(array));
    for (uint64_t load_index = 0; load_index < loads.size; load_index++)
    {
        uint64_t load = loads.elements[load_index];
        int found_flag = 0;
        for (uint64_t i = 0; i < load_availabilities->size && !found_flag; i++)
        {
            if (load < load_availabilities->elements[i])
            {
                load_availabilities->elements[i] -= load;
                found_flag = 1;
            }
        }
        if (!found_flag)
        {
            for (uint64_t i = 0; i < formats_dim; i++)
                assigned_formats_ret[i] = 0;
            for (uint64_t i = 0; i < formats_dim && !found_flag; i++)
            {
                if (formats[i].line_rate >= load || i == formats_dim - 1)
                {
                    assigned_formats_ret[i]++;
                    add_element(load_availabilities, load > formats[i].line_rate ? 0 : formats[i].line_rate - load);
                    load -= formats[i].line_rate;
                    if (load < 0)
                        found_flag = 1;
                }
            }
        }
    }
    free(load_availabilities->elements);
    free(load_availabilities);
}

int is_sufficient_assignment(array loads, const modulation_format *formats, uint64_t formats_dim, uint64_t *assigned_formats)
{
    uint64_t assigned_formats_count = 0;
    for (uint64_t i = 0; i < formats_dim; i++)
    {
        assigned_formats_count += assigned_formats[i];
    }
    uint64_t *load_availabilities = malloc(sizeof(uint64_t) * assigned_formats_count);
    for (uint64_t i = 0, index = 0; i < formats_dim; i++)
    {
        for (uint64_t j = 0; j < assigned_formats[i]; j++)
            load_availabilities[index++] = formats[i].line_rate;
    }

    for (uint64_t i = 0; i < loads.size; i++)
    {
        uint64_t load = loads.elements[i];
        int filled_flag = 0;
        for (uint64_t j = 0; j < assigned_formats_count && filled_flag; j++)
        {
            if (load_availabilities[j] >= load)
            {
                load_availabilities[j] -= load;
                filled_flag = 1;
            }
        }
        if (!filled_flag)
        {
            free(load_availabilities);
            return 0;
        }
    }
    free(load_availabilities);
    return 1;
}

void add_slots_to_map(array slots, char *map)
{
    for (uint64_t i = 0; i < slots.size; i++)
    {
        SET_BIT(map, slots.elements[i]);
    }
}

__ssize_t find_least_frequent_index(const uint64_t *slot_assignment_frequency, const char *frequency_map, uint64_t dims)
{
    __ssize_t least_frequent_index = -1;
    __ssize_t current_frequency = -1;
    for (__ssize_t i = 0; i < dims; i++)
    {
        if (!IS_BIT_SET(frequency_map, i) && (least_frequent_index == -1 || slot_assignment_frequency[i] < current_frequency))
        {
            current_frequency = slot_assignment_frequency[i];
            least_frequent_index = i;
        }
    }
    return least_frequent_index;
}

void assign_modulation_formats_and_spectra(const network_t *network, const modulation_format *formats, uint64_t formats_dim, const routing_assignment *assignments, uint64_t assignments_dim, uint64_t *assigned_formats_ret, uint64_t *assigned_spectra_ret)
{
    uint64_t max_links_in_network = network->node_count * network->node_count;

    array *link_loads = calloc(max_links_in_network, sizeof(array));
    array *slot_assignments = calloc(max_links_in_network, sizeof(array));
    uint64_t *slot_assignment_frequency = calloc(MAX_SPECTRAL_SLOTS, sizeof(uint64_t));
    for (uint64_t assignment_index = 0; assignment_index < assignments_dim; assignment_index++)
    {
        routing_assignment assignment = assignments[assignment_index];
        char *unavailable_slots = calloc(MAX_SPECTRAL_SLOTS, sizeof(char));
        for (uint64_t i = 0; i < assignment.path->length; i++)
        {
            uint64_t from = assignment.path->nodes[i], to = assignment.path->nodes[i + 1];
            add_element(link_loads + from * network->node_count + to, assignment.load);
            add_slots_to_map(slot_assignments[from * network->node_count + to], unavailable_slots);
            if (!is_sufficient_assignment(link_loads[from * network->node_count + to], formats, formats_dim, assigned_formats_ret + (from * network->node_count + to) * formats_dim))
            {
                solve_modulation_assignment(link_loads[from * network->node_count + to], formats, formats_dim, assigned_formats_ret + (from * network->node_count + to) * formats_dim);
            }
        }
        __ssize_t slot = find_least_frequent_index(slot_assignment_frequency, unavailable_slots, MAX_SPECTRAL_SLOTS);
        if (slot == -1)
            exit(1);
        slot_assignment_frequency[slot]++;
        for (uint64_t i = 0; i < assignment.path->length; i++)
        {
            uint64_t from = assignment.path->nodes[i], to = assignment.path->nodes[i + 1];
            add_element(slot_assignments + from * network->node_count + to, slot);
        }
        free(unavailable_slots);
    }
    for (uint64_t i = 0; i < max_links_in_network; i++)
        if (link_loads[i].size > 0)
            free(link_loads[i].elements);
    free(link_loads);
    for (uint64_t i = 0; i < max_links_in_network; i++)
        if (slot_assignments[i].size > 0)
            free(slot_assignments[i].elements);
    free(slot_assignment_frequency);
    free(slot_assignments);
}
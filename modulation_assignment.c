#include "modulation_assignment.h"
#include <stdlib.h>

void solve_modulation_assignment(__ssize_t load, const modulation_format *formats, uint64_t formats_dim, uint64_t *assigned_formats_ret)
{
    while (load > 0)
    {
        for (uint64_t i = 0; i < formats_dim; i++)
        {
            if (formats[i].line_rate >= load || i == formats_dim - 1)
            {
                assigned_formats_ret[i]++;
                load -= formats[i].line_rate;
                if (load < 0)
                    break;
            }
        }
    }
}

void assign_modulation_formats(const network_t *network, const modulation_format *formats, uint64_t formats_dim, const routing_assignment *assignments, uint64_t assignments_dim, uint64_t *assigned_formats_ret)
{
    uint64_t *total_link_loads = calloc(network->node_count * network->node_count, sizeof(uint64_t));
    for (uint64_t i = 0; i < assignments_dim; i++)
    {
        routing_assignment assignment = assignments[i];
        for (uint64_t node = 0; node < assignment.path->length; node++)
        {
            total_link_loads[assignment.path->nodes[node] * network->node_count + assignment.path->nodes[node + 1]] += assignment.load;
        }
    }
    for (uint64_t i = 0; i < network->node_count; i++)
    {
        for (uint64_t j = 0; j < network->node_count; j++)
        {
            if (total_link_loads[i * network->node_count + j] > 0)
                solve_modulation_assignment(total_link_loads[i * network->node_count + j], formats, formats_dim, assigned_formats_ret + (i * network->node_count + j) * formats_dim);
            // The matrix is symmetric
            total_link_loads[j * network->node_count + i] = 0;
        }
    }
    free(total_link_loads);
}
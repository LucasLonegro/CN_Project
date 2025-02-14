
#include "routing_solution.h"
#include <stdlib.h>

/**
 * @brief A function that receives a network, a set of assigned pathing requests, a connection request, the index of that request, the total number of connection requests, and an abstract data structure it may modify
 * @return The index of the next connection request it wants to observe, -1 if done
 *
 */
typedef __ssize_t (*assigner)(const network_t *network, routing_assignment *current_assignments, connection_request request, uint64_t request_index, uint64_t request_count, void *data);

__ssize_t fixed_shortest_path(const network_t *network, routing_assignment *current_assignments, connection_request request, uint64_t request_index, uint64_t request_count, void *data)
{
    if(request_index >= request_count)
        return -1;
    current_assignments[request_index].load = request.load;
    current_assignments[request_index].path = weighted_distances(network, request.from_node_id)[request.to_node_id];
    return request_index + 1;
}

void run_routing_algorithm(const network_t *network, connection_request *requests, uint64_t requests_dim, void *data, assigner algorithm, routing_assignment *assignments_ret)
{
    uint64_t index = 0;
    while (index != -1)
    {
        index = algorithm(network, assignments_ret, requests[index], index, requests_dim, data);
    }
    return;
}

void generate_routing(const network_t *network, connection_request *requests, uint64_t requests_dim, routing_assignment *assignments_ret)
{
    run_routing_algorithm(network, requests, requests_dim, NULL, fixed_shortest_path, assignments_ret);
    return;
}

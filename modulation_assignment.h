#ifndef MDLN_H
#define MDLN_H
#include <stdint.h>
#include "problem.h"
#include "network.h"
#include "routing_solution.h"

/**
 * @brief Assigns the necessary modulation formats to links and spectra to paths in a network, given a set of routing assignments over that network
 *
 * @param network
 * @param formats Sorted from lowest to highest load capacity
 * @param formats_dim
 * @param assignments
 * @param assignments_dim
 * @param assigned_formats_ret Return variable. An array with formats_dim spaces times the square of the amount of nodes in the network.
 * @param assigned_spectra_ret Return variable. An array with assignments_dim spaces.
 * @note assigned_formats_ret[(i + j * network->nodes_count) * formats_dim + k] represents the amount of modulation format of index k necessary for the link from i to j, always 0 if the link does not exist in the network
 */
void assign_modulation_formats_and_spectra(const network_t *network, const modulation_format *formats, uint64_t formats_dim, const routing_assignment *assignments, uint64_t assignments_dim, uint64_t *assigned_formats_ret, uint64_t *assigned_spectra_ret);

#endif

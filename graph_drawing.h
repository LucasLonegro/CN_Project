#ifndef GRPHD_H
#define GRPHD_H
#include <stdio.h>
#include "network.h"

#define SPRING_CONSTANT 0.0001
#define MAX_ITERATIONS 8001

typedef struct coordinate
{
    double x;
    double y;
} coordinate;

void spring_embedder(const network_t *network, double max_velocity, double scale, coordinate *coordinates_ret);

void print_graph(const network_t *network, const coordinate *coordinates, FILE *file);

#endif

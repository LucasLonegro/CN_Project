#ifndef GRPHD_H
#define GRPHD_H
#include <stdio.h>
#include "network.h"

#define GRAVITATIONAL_CONSTANT 1.1
#define REPULSIVE_CONSTANT 100.0
#define DESIRED_DISTANCE 150.0
#define MASS 3.0
#define MAX_ITERATIONS 1000000

typedef struct coordinate
{
    double x;
    double y;
} coordinate;

void spring_embedder(const network_t *network, double max_velocity, double scale, coordinate *coordinates_ret);

void print_graph(const network_t *network, const coordinate *coordinates, FILE *file);

#endif

#include "graph_drawing.h"
#include "math.h"
#include <stdlib.h>

typedef struct vector
{
    double x;
    double y;
} vector;

double r2()
{
    return (double)rand() / (double)RAND_MAX;
}

double dabs(double d)
{
    return d > 0 ? d : -d;
}

double update(const network_t *network, vector *velocities, double scale, coordinate *coordinates_ret, uint64_t *degrees, double temperature)
{
    for (uint64_t i = 0; i < network->node_count; i++)
    {
        for (uint64_t j = 0; j < network->node_count; j++)
        {
            __ssize_t natural_length;
            if ((natural_length = get_link_weight(network, i, j)) != -1)
            {
                double dx = coordinates_ret[i].x - coordinates_ret[j].x;
                double dy = coordinates_ret[i].y - coordinates_ret[j].y;
                double total_force = SPRING_CONSTANT * ((sqrt(pow(dx, 2) + pow(dy, 2))) - 10);

                double angle = atan(dy / dx);
                double fx = total_force * cos(angle) * (dx < 0 ? 1 : -1);
                double fy = total_force * sin(angle) * (dx < 0 ? 1 : -1);
                velocities[i].x += fx / degrees[i];
                velocities[i].y += fy / degrees[i];
            }
        }
    }

    for (uint64_t i = 0; i < network->node_count; i++)
    {
            double dx = coordinates_ret[i].x - 5;
            double dy = coordinates_ret[i].y - 5;
            double total_force = sqrt(pow(dx, 2) + pow(dy, 2)) * 0.0001;

            double angle = atan(dy / dx);
            double fx = total_force * cos(angle) * (dx < 0 ? 1 : -1);
            double fy = total_force * sin(angle) * (dx < 0 ? 1 : -1);
            velocities[i].x += fx / degrees[i];
            velocities[i].y += fy / degrees[i];
    }

    double max_velocity = 0;
    for (uint64_t i = 0; i < network->node_count; i++)
    {
        coordinates_ret[i].x += velocities[i].x;
        coordinates_ret[i].y += velocities[i].y;
        double cumulative_velocity = velocities[i].x + velocities[i].y;
        if (dabs(cumulative_velocity) > dabs(max_velocity))
            max_velocity = cumulative_velocity;
    }
    return max_velocity;
}

void spring_embedder(const network_t *network, double max_velocity, double scale, coordinate *coordinates_ret)
{
    srand((uint64_t)coordinates_ret);
    vector *velocities = calloc(network->node_count, sizeof(vector));
    uint64_t *degrees = calloc(network->node_count, sizeof(uint64_t));

    for (uint64_t i = 0; i < network->node_count; i++)
    {
        coordinates_ret[i].x = i + 10 * r2();
        coordinates_ret[i].y = i + 10 * r2();
    }

    for (uint64_t i = 0; i < network->node_count; i++)
    {
        for (uint64_t j = 0; j < network->node_count; j++)
            if (get_link_weight(network, i, j) != -1)
                degrees[i]++;
    }

    uint64_t iterations = 0;
    double temperature = 1;
    double current_max_velocity = max_velocity;
    while (dabs(current_max_velocity) >= max_velocity && iterations++ < MAX_ITERATIONS)
    {
        current_max_velocity = update(network, velocities, scale, coordinates_ret, degrees, temperature);
        temperature *= 0.9995;
    }
    printf("cmv: %g\n", current_max_velocity);
    if (iterations >= MAX_ITERATIONS)
        printf("EXCEEDED MAX ITERATIONS\n");
    for (uint64_t i = 0; i < network->node_count; i++)
    {
        coordinates_ret[i].x *= scale;
        coordinates_ret[i].y *= scale;
    }
}

void print_graph(const network_t *network, const coordinate *coordinates, FILE *file)
{
    fprintf(file, "\\begin{tikzpicture}[node distance=2cm,>=stealth',auto, every place/.style={draw}, baseline]\n");
    for (uint64_t i = 0; i < network->node_count; i++)
    {
        fprintf(file, "\\coordinate(C%ld) at (%g,%g);\n", i + 1, coordinates[i].x, coordinates[i].y);
        fprintf(file, "\\node [place] (S%ld) [right=of C%ld] {%ld};\n", i + 1, i + 1, i + 1);
    }
    for (uint64_t i = 0; i < network->node_count; i++)
    {
        for (uint64_t j = 0; j < network->node_count; j++)
        {
            __ssize_t link_weight = get_link_weight(network, i, j);
            if (link_weight != -1)
            {
                fprintf(file, "\\path[->] (S%ld) edge node {%ld} (S%ld);\n", i + 1, link_weight, j + 1);
            }
        }
    }
    fprintf(file, " \\end{tikzpicture}\n");
}

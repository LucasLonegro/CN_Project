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

double update(const network_t *network, vector *velocities, const uint64_t *degrees, uint64_t central_node, double scale, coordinate *coordinates_ret)
{
    for (uint64_t i = 0; i < network->node_count; i++)
    {
        velocities[i].x += -1 * (coordinates_ret[i].x) * (GRAVITATIONAL_CONSTANT);
        velocities[i].y += -1 * (coordinates_ret[i].y) * (GRAVITATIONAL_CONSTANT);
        // printf("%ld: x: %.3g\tfx: %.3g\t vx: %.3g\n", i, -1 * coordinates_ret[i].x, coordinates_ret[i].x * (GRAVITATIONAL_CONSTANT), velocities[i].x);
        // printf("%ld: y: %.3g\tfy: %.3g\t vy: %.3g\n", i, -1 * coordinates_ret[i].y, coordinates_ret[i].y * (GRAVITATIONAL_CONSTANT), velocities[i].y);
        //  velocities[i].x -= scale / MASS / 10 * r2();
        //  velocities[i].y -= scale / MASS / 10 * r2();

        for (uint64_t j = 0; j < network->node_count; j++)
        {
            if (i == j)
                continue;
            double dx = coordinates_ret[i].x - coordinates_ret[j].x;
            double dy = coordinates_ret[i].y - coordinates_ret[j].y;
            double d = sqrt(pow(dx, 2) + pow(dy, 2));
            double fx = REPULSIVE_CONSTANT * dx / (d * d);
            double fy = REPULSIVE_CONSTANT * dy / (d * d);
            velocities[i].x += -fx;
            velocities[j].x -= -fx;
            velocities[i].y += -fy;
            velocities[j].y -= -fy;

            __ssize_t natural_distance;
            if (i > j && (natural_distance = get_link_weight(network, i, j)) != -1)
            {
                velocities[i].x += -dx * 0.2 / degrees[i];
                velocities[j].x -= -dx * 0.2 / degrees[i];
                velocities[i].y += -dy * 0.2 / degrees[i];
                velocities[j].y -= -dy * 0.2 / degrees[i];
            }
        }
    }

    double max_velocity = 0;
    for (uint64_t i = 0; i < network->node_count; i++)
    {
        if(i == central_node)
            continue;
        coordinates_ret[i].x += velocities[i].x / MASS;
        coordinates_ret[i].y += velocities[i].y / MASS;
        double cumulative_velocity = sqrt(pow(velocities[i].x / MASS, 2) + pow(velocities[i].y / MASS, 2));
        if (dabs(cumulative_velocity) > dabs(max_velocity))
        {
            // printf("i: %ld; vx: %.3g; vy: %.3g\n", i, velocities[i].x, velocities[i].y);
            max_velocity = cumulative_velocity;
        }
        velocities[i].x = 0;
        velocities[i].y = 0;
    }
    return max_velocity;
}

void spring_embedder(const network_t *network, double max_velocity, double scale, coordinate *coordinates_ret)
{
    srand((uint64_t)coordinates_ret);
    vector *velocities = calloc(network->node_count, sizeof(vector));
    uint64_t *degrees = calloc(network->node_count, sizeof(uint64_t));

    uint64_t ignore_index = 0;
    for (uint64_t i = 0; i < network->node_count; i++)
    {
        for (uint64_t j = 0; j < network->node_count; j++)
        {
            if (get_link_weight(network, i, j) != -1)
            {
                degrees[i]++;
                if (degrees[i] > degrees[ignore_index])
                ignore_index = i;
            }
        }
    }
    for (uint64_t i = 0; i < network->node_count; i++)
    {
        coordinates_ret[i].x = 500 * r2();
        coordinates_ret[i].y = 500 * r2();
        if(i == ignore_index){
            coordinates_ret[i].x = 0;
            coordinates_ret[i].y = 0;
        }
    }
    
    uint64_t iterations = 0;
    double current_max_velocity = max_velocity;
    while (dabs(current_max_velocity) >= max_velocity && iterations++ < MAX_ITERATIONS)
    {
        // printf("\n\n");
        current_max_velocity = update(network, velocities, degrees, ignore_index, scale, coordinates_ret);
    }
    printf("cmv: %.3g\n", current_max_velocity);
    if (iterations >= MAX_ITERATIONS)
        printf("EXCEEDED MAX ITERATIONS\n");
    /*
        double max = 0, min = -1;
        for (uint64_t i = 0; i < network->node_count; i++)
        {
            if (coordinates_ret[i].x > max)
            {
                max = coordinates_ret[i].x;
            }
            if (coordinates_ret[i].y > max)
            {
                max = coordinates_ret[i].y;
            }
            if (coordinates_ret[i].x < min)
            {
                min = coordinates_ret[i].x;
            }
            if (coordinates_ret[i].y < min)
            {
                min = coordinates_ret[i].y;
            }
        }
        for (uint64_t i = 0; i < network->node_count; i++)
        {
            coordinates_ret[i].x += min;
            coordinates_ret[i].x *= scale / max;
            coordinates_ret[i].y += min;
            coordinates_ret[i].y *= scale / max;
        }*/
}

void print_graph(const network_t *network, const coordinate *coordinates, FILE *file)
{
    fprintf(file, "\\begin{tikzpicture}[node distance=2cm,>=stealth',auto, every place/.style={draw}, baseline]\n");
    for (uint64_t i = 0; i < network->node_count; i++)
    {
        fprintf(file, "\\coordinate(C%ld) at (%.3g,%.3g);\n", i + 1, coordinates[i].x, coordinates[i].y);
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

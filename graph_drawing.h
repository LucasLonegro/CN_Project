#ifndef GRPHD_H
#define GRPHD_H
#include <stdio.h>
#include "network.h"

typedef struct coordinate
{
    double x;
    double y;
} coordinate;

typedef enum line_t
{
    DOTTED_LINE,
    DASHED_LINE,
    SOLID_LINE
} line_t;

typedef enum offset_t
{
    OFFSET_UP,
    OFFSET_DOWN,
    NO_OFFSET
} offset_t;

typedef struct path_drawing
{
    line_t line;
    offset_t offset;
    path_t *path;
} path_drawing;

typedef struct bar_plot
{
    uint64_t samples_count;
    uint64_t data_points_count;
    coordinate *data_points;
    char **legends;
    char *y_label;
} bar_plot;

typedef char *(*label_generator)(void *data, uint64_t from, uint64_t to);

void print_graph(const network_t *network, label_generator labeler, void *data, const coordinate *node_coordinates, path_drawing *paths, uint64_t paths_size, double x_scale, double y_scale, FILE *file);

void print_plot(bar_plot plot, FILE *file);

void print_prologue(FILE *file, char *title);
void print_section(FILE *file, char *section);
void print_epilogue(FILE *file);

#endif

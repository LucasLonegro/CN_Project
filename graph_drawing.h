#ifndef GRPHD_H
#define GRPHD_H
#include <stdio.h>
#include "network.h"

typedef char *(*label_generator)(const void *data, uint64_t from, uint64_t to);

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

typedef struct hex_color
{
    unsigned char red;
    unsigned char green;
    unsigned char blue;
} hex_color;

typedef struct path_drawing
{
    line_t line;
    offset_t offset;
    const path_t *path;
    hex_color color;
    label_generator labeler;
    void *data;
} path_drawing;

typedef struct data_point
{
    double y;
    char *x;
} data_point;

typedef struct bar_plot
{
    uint64_t samples_count;
    uint64_t data_points_count;
    data_point *data_points;
    char **x_labels;
    uint64_t x_labels_count;
    char **legends;
    char *y_label;
    double width;
} bar_plot;

typedef enum print_graph_options{
    DRAW_ARROWS = 1<<0,
    REVERSE_ARROWS = 1<<1
}print_graph_options;

void print_graph(const network_t *network, label_generator labeler, void *data, const coordinate *node_coordinates, path_drawing *paths, uint64_t paths_size, double x_scale, double y_scale, FILE *file, print_graph_options options);

void print_plot(bar_plot plot, FILE *file);

void print_prologue(FILE *file, char *title);
void print_linebreak(FILE *file);
void print_section(FILE *file, char *section);
void print_epilogue(FILE *file);

#endif

#ifndef GRPHD_H
#define GRPHD_H
#include <stdio.h>
#include "network.h"
#include "utils.h"

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

typedef hex_color (*edge_color_generator)(const void *data, uint64_t from, uint64_t to);

typedef struct edge_style
{
    label_generator labeler;
    const void *label_data;
    edge_color_generator colorer;
    const void *color_data;
} edge_style;

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
    char *x_label;
    double width;
} bar_plot;

typedef struct spectrum_map
{
    uint64_t row_count;
    char **row_labels;
    const dynamic_char_array **rows;
    uint64_t slot_count;
    uint64_t status_count;
    const char **status_names;
    const hex_color *status_colors;
    uint64_t skip_status;
    double width;
} spectrum_map;

typedef enum print_graph_options
{
    DRAW_ARROWS = 1 << 0,
    REVERSE_ARROWS = 1 << 1,
    BIDIRECTIONAL = 1 << 2,
    FIT_TO_PAGE = 1 << 3,
    THICK_EDGES = 1 << 4
} print_graph_options;

void print_graph(const network_t *network, edge_style edges, const coordinate *node_coordinates, path_drawing *paths, uint64_t paths_size, double x_scale, double y_scale, FILE *file, print_graph_options options);

void print_plot(bar_plot plot, FILE *file);

void print_spectrum_map(spectrum_map map, FILE *file);

hex_color color_scale(double t);
void print_color_scale_legend(FILE *file, double min, double max, const char *label);

void print_latex_escaped(FILE *file, const char *text);
void print_prologue(FILE *file, const char *title);
void print_linebreak(FILE *file);
void print_section(FILE *file, const char *section);
void print_pagebreak(FILE *file);
void print_epilogue(FILE *file);

#endif

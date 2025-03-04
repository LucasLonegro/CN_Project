#ifndef PRBLM_H
#define PRBLM_H
#include <stdint.h>
#include "graph_drawing.h"

#define ITALIAN_TOPOLOGY_SIZE 10
#define ITALIAN_LINKS_SIZE 30
#define GERMAN_TOPOLOGY_SIZE 7
#define GERMAN_LINKS_SIZE 22
#define MODULATION_FORMATS_DIM 3
#define MAX_SPECTRAL_SLOTS 320
#define FSU_BANDWIDTH 12.5

typedef struct modulation_format
{
    uint64_t line_rate;
    double channel_bandwidth;
    uint64_t maximum_length;
    double cost;
    char *name;
} modulation_format;

extern modulation_format formats[MODULATION_FORMATS_DIM];

extern coordinate italian_coordinates[ITALIAN_TOPOLOGY_SIZE];
extern coordinate german_coordinates[GERMAN_TOPOLOGY_SIZE];

extern uint64_t german_links[GERMAN_LINKS_SIZE][3];
extern uint64_t g7_1[GERMAN_TOPOLOGY_SIZE][GERMAN_TOPOLOGY_SIZE];
extern uint64_t g7_2[GERMAN_TOPOLOGY_SIZE][GERMAN_TOPOLOGY_SIZE];
extern uint64_t g7_3[GERMAN_TOPOLOGY_SIZE][GERMAN_TOPOLOGY_SIZE];
extern uint64_t g7_4[GERMAN_TOPOLOGY_SIZE][GERMAN_TOPOLOGY_SIZE];
extern uint64_t g7_5[GERMAN_TOPOLOGY_SIZE][GERMAN_TOPOLOGY_SIZE];

extern uint64_t italian_links[ITALIAN_LINKS_SIZE][3];
extern uint64_t IT10_1[ITALIAN_TOPOLOGY_SIZE][ITALIAN_TOPOLOGY_SIZE];
extern uint64_t IT10_2[ITALIAN_TOPOLOGY_SIZE][ITALIAN_TOPOLOGY_SIZE];
extern uint64_t IT10_3[ITALIAN_TOPOLOGY_SIZE][ITALIAN_TOPOLOGY_SIZE];
extern uint64_t IT10_4[ITALIAN_TOPOLOGY_SIZE][ITALIAN_TOPOLOGY_SIZE];
extern uint64_t IT10_5[ITALIAN_TOPOLOGY_SIZE][ITALIAN_TOPOLOGY_SIZE];

#endif

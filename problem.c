#include "problem.h"

 modulation_format formats[MODULATION_FORMATS_DIM] = {{.line_rate = 100, .channel_bandwidth = 37.5, .maximum_length = 2000, .cost = 1.5},
                                                     {.line_rate = 200, .channel_bandwidth = 37.5, .maximum_length = 700, .cost = 2.0},
                                                     {.line_rate = 400, .channel_bandwidth = 75.0, .maximum_length = 500, .cost = 3.7}};

 uint64_t german_links[GERMAN_LINKS_SIZE][3] = {{1, 2, 114},
                              {1, 3, 120},
                              {2, 3, 157},
                              {2, 4, 306},
                              {3, 4, 298},
                              {3, 5, 258},
                              {3, 6, 316},
                              {4, 5, 174},
                              {5, 6, 353},
                              {5, 7, 275},
                              {6, 7, 224},
                              {2, 1, 114},
                              {3, 1, 120},
                              {3, 2, 157},
                              {4, 2, 306},
                              {4, 3, 298},
                              {5, 3, 258},
                              {6, 3, 316},
                              {5, 4, 174},
                              {6, 5, 353},
                              {7, 5, 275},
                              {7, 6, 224}};

 uint64_t g7_1[GERMAN_TOPOLOGY_SIZE][GERMAN_TOPOLOGY_SIZE] = {
    {0, 7, 10, 11, 0, 29, 18},
    {14, 0, 15, 28, 17, 8, 10},
    {0, 12, 0, 1, 12, 1, 5},
    {11, 16, 33, 0, 8, 8, 20},
    {6, 22, 13, 14, 0, 8, 9},
    {0, 29, 6, 16, 15, 0, 19},
    {28, 10, 8, 33, 1, 6, 0}};
 uint64_t g7_2[GERMAN_TOPOLOGY_SIZE][GERMAN_TOPOLOGY_SIZE] = {
    {0, 35, 33, 29, 40, 58, 53},
    {70, 0, 49, 58, 39, 30, 21},
    {13, 46, 0, 45, 37, 53, 7},
    {23, 21, 62, 0, 16, 36, 36},
    {45, 34, 42, 58, 0, 33, 18},
    {28, 75, 53, 36, 18, 0, 52},
    {45, 12, 30, 73, 40, 18, 0}};
 uint64_t g7_3[GERMAN_TOPOLOGY_SIZE][GERMAN_TOPOLOGY_SIZE] = {
    {0, 76, 50, 80, 78, 71, 89},
    {96, 0, 82, 86, 74, 66, 51},
    {57, 66, 0, 103, 102, 137, 31},
    {72, 61, 77, 0, 73, 51, 78},
    {94, 69, 87, 84, 0, 59, 69},
    {58, 96, 99, 102, 65, 0, 99},
    {85, 68, 64, 105, 107, 67, 0}};
 uint64_t g7_4[GERMAN_TOPOLOGY_SIZE][GERMAN_TOPOLOGY_SIZE] = {
    {0, 101, 107, 139, 129, 123, 153},
    {130, 0, 103, 135, 124, 121, 91},
    {129, 136, 0, 182, 165, 180, 87},
    {160, 125, 160, 0, 130, 101, 135},
    {128, 112, 137, 123, 0, 142, 105},
    {123, 160, 131, 156, 92, 0, 151},
    {186, 97, 71, 162, 172, 108, 0}};
 uint64_t g7_5[GERMAN_TOPOLOGY_SIZE][GERMAN_TOPOLOGY_SIZE] = {
    {0, 164, 149, 223, 183, 190, 205},
    {198, 0, 136, 200, 187, 179, 154},
    {202, 195, 0, 259, 232, 297, 132},
    {220, 190, 229, 0, 182, 160, 185},
    {174, 170, 236, 203, 0, 238, 165},
    {199, 193, 212, 242, 166, 0, 204},
    {273, 207, 109, 230, 224, 181, 0}};

 uint64_t italian_links[ITALIAN_LINKS_SIZE][3] = {{1, 2, 150},
                               {1, 7, 350},
                               {1, 3, 210},
                               {2, 7, 310},
                               {2, 4, 420},
                               {3, 5, 200},
                               {4, 8, 460},
                               {5, 7, 90},
                               {5, 6, 100},
                               {6, 9, 270},
                               {6, 7, 210},
                               {7, 8, 180},
                               {7, 9, 200},
                               {8, 10, 120},
                               {9, 10, 170},
                               {2, 1, 150},
                               {7, 1, 350},
                               {3, 1, 210},
                               {7, 2, 310},
                               {4, 2, 420},
                               {5, 3, 200},
                               {8, 4, 460},
                               {7, 5, 90},
                               {6, 5, 100},
                               {9, 6, 270},
                               {7, 6, 210},
                               {8, 7, 180},
                               {9, 7, 200},
                               {10, 8, 120},
                               {10, 9, 170}};

 uint64_t IT10_1[ITALIAN_TOPOLOGY_SIZE][ITALIAN_TOPOLOGY_SIZE] = {
    {0, 1, 13, 0, 12, 11, 4, 10, 3, 0},
    {16, 0, 0, 7, 5, 0, 17, 11, 8, 10},
    {11, 0, 0, 9, 0, 18, 23, 7, 4, 0},
    {8, 0, 8, 0, 8, 0, 13, 9, 10, 0},
    {0, 25, 10, 14, 0, 0, 4, 11, 0, 0},
    {4, 0, 3, 9, 5, 0, 7, 3, 10, 0},
    {0, 10, 15, 18, 0, 7, 0, 0, 9, 12},
    {0, 0, 0, 0, 1, 17, 10, 0, 0, 0},
    {9, 0, 2, 10, 14, 1, 7, 7, 0, 1},
    {0, 0, 0, 0, 0, 0, 9, 4, 6, 0}};
 uint64_t IT10_2[ITALIAN_TOPOLOGY_SIZE][ITALIAN_TOPOLOGY_SIZE] = {
    {0, 7, 24, 6, 33, 20, 6, 10, 3, 10},
    {25, 0, 11, 27, 9, 34, 27, 22, 23, 13},
    {36, 0, 0, 30, 18, 35, 25, 7, 18, 13},
    {14, 1, 27, 0, 26, 6, 21, 9, 35, 2},
    {38, 55, 28, 16, 0, 10, 20, 21, 8, 14},
    {14, 0, 27, 19, 30, 0, 19, 3, 24, 5},
    {10, 22, 31, 18, 7, 9, 0, 3, 35, 44},
    {0, 16, 6, 0, 1, 31, 16, 0, 21, 32},
    {21, 6, 5, 41, 15, 20, 32, 32, 0, 19},
    {9, 12, 6, 9, 42, 34, 20, 17, 6, 0}};
 uint64_t IT10_3[ITALIAN_TOPOLOGY_SIZE][ITALIAN_TOPOLOGY_SIZE] = {
    {0, 7, 46, 6, 70, 35, 19, 43, 14, 14},
    {25, 0, 28, 30, 57, 65, 33, 31, 49, 19},
    {69, 6, 0, 40, 46, 44, 39, 40, 28, 56},
    {36, 33, 30, 0, 48, 33, 48, 39, 49, 12},
    {77, 72, 67, 24, 0, 25, 26, 35, 17, 28},
    {28, 31, 42, 27, 53, 0, 34, 17, 43, 31},
    {33, 31, 59, 40, 17, 34, 0, 36, 67, 49},
    {0, 32, 33, 10, 20, 45, 29, 0, 48, 66},
    {35, 15, 15, 54, 38, 36, 46, 40, 0, 41},
    {29, 55, 25, 36, 48, 45, 31, 31, 36, 0}};
 uint64_t IT10_4[ITALIAN_TOPOLOGY_SIZE][ITALIAN_TOPOLOGY_SIZE] = {
    {0, 73, 73, 30, 89, 71, 29, 98, 35, 33},
    {31, 0, 39, 72, 74, 73, 47, 44, 84, 53},
    {78, 17, 0, 66, 81, 46, 73, 66, 55, 88},
    {67, 50, 54, 0, 68, 51, 63, 69, 96, 23},
    {102, 122, 98, 26, 0, 58, 73, 40, 36, 55},
    {95, 58, 59, 39, 62, 0, 45, 43, 67, 53},
    {54, 53, 84, 52, 31, 57, 0, 64, 83, 74},
    {32, 64, 60, 32, 28, 63, 61, 0, 88, 79},
    {90, 38, 22, 72, 54, 45, 57, 50, 0, 58},
    {36, 70, 59, 70, 93, 60, 51, 80, 59, 0}};
 uint64_t IT10_5[ITALIAN_TOPOLOGY_SIZE][ITALIAN_TOPOLOGY_SIZE] = {
    {0, 105, 132, 76, 125, 117, 67, 133, 53, 71},
    {67, 0, 58, 107, 95, 104, 51, 81, 95, 70},
    {122, 41, 0, 119, 101, 85, 118, 90, 109, 113},
    {98, 77, 72, 0, 116, 65, 92, 113, 145, 57},
    {127, 138, 126, 55, 0, 89, 97, 57, 59, 81},
    {155, 79, 89, 53, 81, 0, 68, 56, 99, 69},
    {124, 77, 129, 70, 38, 132, 0, 92, 88, 89},
    {51, 92, 97, 62, 74, 89, 102, 0, 150, 103},
    {154, 50, 49, 105, 71, 68, 121, 79, 0, 102},
    {59, 87, 59, 95, 134, 60, 104, 98, 103, 0}};

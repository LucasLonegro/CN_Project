#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <stdlib.h>
#define IS_BIT_SET(x, i) (((x)[(i) >> 3] & (1 << ((i) & 7))) != 0)
#define SET_BIT(x, i) (x)[(i) >> 3] |= (1 << ((i) & 7))
#define CLEAR_BIT(x, i) (x)[(i) >> 3] &= (1 << ((i) & 7)) ^ 0xFF

typedef struct dynamic_bit_array
{
    char *bits;
    uint64_t size;
} dynamic_bit_array;

typedef struct dynamic_char_array{
    char *elements;
    uint64_t size;
} dynamic_char_array;

void set_bit(dynamic_bit_array *array, uint64_t index);
void unset_bit(dynamic_bit_array *array, uint64_t index);
int is_bit_set(const dynamic_bit_array *array, uint64_t index);
dynamic_bit_array *new_dynamic_bit_array();
void free_dynamic_bit_array(dynamic_bit_array *array);


void set_element(dynamic_char_array *array, uint64_t index, char value);
char get_element(const dynamic_char_array *array, uint64_t index);
dynamic_char_array *new_dynamic_char_array();
void free_dynamic_char_array(dynamic_char_array *array);

#endif
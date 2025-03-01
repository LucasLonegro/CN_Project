#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <stdlib.h>

typedef struct dynamic_char_array
{
    char *elements;
    uint64_t size;
} dynamic_char_array;

typedef struct dynamic_word_array
{
    uint64_t *elements;
    uint64_t size;
} dynamic_word_array;

void set_element(dynamic_char_array *array, uint64_t index, char value);
char get_element(const dynamic_char_array *array, uint64_t index);
dynamic_char_array *new_dynamic_char_array();
void free_dynamic_char_array(dynamic_char_array *array);

void set_value(dynamic_word_array *array, uint64_t index, char value);
char get_value(const dynamic_word_array *array, uint64_t index);
dynamic_word_array *new_dynamic_word_array();
void free_dynamic_word_array(dynamic_word_array *array);

#endif
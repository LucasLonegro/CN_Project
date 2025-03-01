#include "utils.h"

void set_element(dynamic_char_array *array, uint64_t index, char value)
{
    if (array->size <= index)
    {
        array->elements = realloc(array->elements, (index + 1) * sizeof(char));
        for (uint64_t i = array->size; i < index + 1; i++)
            array->elements[i] = 0;
        array->size = index + 1;
    }
    array->elements[index] = value;
}
char get_element(const dynamic_char_array *array, uint64_t index)
{
    if (array->size <= index)
        return 0;
    return array->elements[index];
}
dynamic_char_array *new_dynamic_char_array()
{
    dynamic_char_array *new_dynamic_char_array = malloc(sizeof(dynamic_char_array));
    new_dynamic_char_array->elements = NULL;
    new_dynamic_char_array->size = 0;
    return new_dynamic_char_array;
}
void free_dynamic_char_array(dynamic_char_array *array)
{
    if (array->size > 0 && array->elements != NULL)
        free(array->elements);
    free(array);
}

void set_value(dynamic_word_array *array, uint64_t index, char value)
{
    if (array->size <= index)
    {
        array->elements = realloc(array->elements, (index + 1) * sizeof(uint64_t));
        for (uint64_t i = array->size; i < index + 1; i++)
            array->elements[i] = 0;
        array->size = index + 1;
    }
    array->elements[index] = value;
}
char get_value(const dynamic_word_array *array, uint64_t index)
{
    if (array->size <= index)
        return 0;
    return array->elements[index];
}

dynamic_word_array *new_dynamic_word_array()
{
    dynamic_word_array *new_dynamic_word_array = malloc(sizeof(dynamic_word_array));
    new_dynamic_word_array->elements = NULL;
    new_dynamic_word_array->size = 0;
    return new_dynamic_word_array;
}
void free_dynamic_word_array(dynamic_word_array *array)
{
    if (array->size > 0 && array->elements != NULL)
        free(array->elements);
    free(array);
}

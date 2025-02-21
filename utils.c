#include "utils.h"

dynamic_bit_array *new_dynamic_bit_array()
{
    dynamic_bit_array *new_dynamic_bit_array = malloc(sizeof(dynamic_bit_array));
    new_dynamic_bit_array->bits = NULL;
    new_dynamic_bit_array->size = 0;
    return new_dynamic_bit_array;
}

void free_dynamic_bit_array(dynamic_bit_array *array)
{
    if (array->size > 0 && array->bits != NULL)
        free(array->bits);
    free(array);
}

void set_bit(dynamic_bit_array *array, uint64_t index)
{
    if ((array->size << 3) <= index)
    {
        uint64_t new_size = (index >> 3) + 1;
        array->bits = realloc(array->bits, new_size * sizeof(char));
        for (uint64_t i = array->size; i < new_size; i++)
            array->bits[i] = 0;
        array->size = new_size;
    }
    SET_BIT(array->bits, index);
}

void unset_bit(dynamic_bit_array *array, uint64_t index)
{
    if ((array->size >> 3) > index)
        CLEAR_BIT(array->bits, index);
}

int is_bit_set(const dynamic_bit_array *array, uint64_t index)
{
    return (array->size << 3) > index && IS_BIT_SET(array->bits, index);
}

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

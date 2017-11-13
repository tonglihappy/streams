#ifndef _UTILS_H
#define _UTILS_H

void *grow_array(void *array, int elem_size, int *size, int new_size);

#define GROW_ARRAY(array, nb_elems)\
	array = grow_array(array, sizeof(*array), &nb_elems, nb_elems + 1)

#endif
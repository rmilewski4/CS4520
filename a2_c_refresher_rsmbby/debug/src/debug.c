#include "../include/debug.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// protected function, that only this .c can use
int comparator_func(const void *a, const void *b) {
    //replaced original function body with simpler to understand logic
    //dereferencing arguments and storing as local variables
    uint16_t arg1 = *(uint16_t*) a;
    uint16_t arg2 = *(uint16_t*) b;
    //following qsort compare function values mentioned in man page
    if(arg1 < arg2) {
        return -1;
    }
    if(arg1 > arg2) {
        return 1;
    }
    return 0;
    //original line
    //return *(uint16_t*)a - *(uint16_t *)b;
}

bool terrible_sort(uint16_t *data_array, const size_t value_count) {
    //checking for bad parameters
    if(data_array == NULL || value_count == 0) {
        return false;
    }
    //was throwing error due to lack of * in variable definition
    uint16_t* sorting_array = malloc(value_count * sizeof(*data_array));
    //fixed by casting value_count to an integer and changing to postincrement counter
    for (int i = 0; i < (int)value_count; i++) {
        sorting_array[i] = data_array[i];
    }
    //made simpler by changing size to the known uint16_t instead of doing division inside the call
    qsort(sorting_array, value_count, sizeof(uint16_t), comparator_func);

    bool sorted = true;
    //fixed this loop similar to above, since we are validating two elements at once, we need to only go one less than the length of the array to avoid going off the edge
    for (int j = 0; j < (int) value_count-1; j++) {
        //Once again using simpler code, checking the two elements and returning false if one is out of order
        if(sorting_array[j] > sorting_array[j+1]) {
            sorted = false;
        }
        //sorted &= sorting_array[j] < sorting_array[j + 1];
    }
    //if properly sorted, we copy the sorting array back into the original array, this was switched in the original function
    if (sorted) {
        memcpy(data_array, sorting_array, value_count*sizeof(*data_array));
    }
    //no free was ever called original, fixed.
    free(sorting_array);
    return sorted;
}


/* findminmax_par1.c - find the min and max values in a random array
 * Comunication via files.
 *
 * usage: ./findminmax <seed> <arraysize> <nprocs>
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "measure.h"

/* a struct used to pass results to caller */
struct results {
    int min;
    int max;
};

/* given an array of ints and the size of array, find min and max values */
struct results find_min_and_max(int *subarray, int n)
{
    int i, min, max;
    min = max = subarray[0];
    struct results r;

    for (i = 1; i < n; i++) {
        if (subarray[i] < min) {
            min = subarray[i];
        }
        if (subarray[i] > max) {
            max = subarray[i];
        }
    }

    r.min = min;
    r.max = max;
    return r;
}

int* generate_random_array(int seed, int size)
{
    int *array;
    char randomstate[8];
    int i;

    /* allocate array and populate with random values */
    array = (int *) malloc(sizeof(int) * size);

    initstate(seed, randomstate, 8);

    for (i = 0; i < size; i++) {
        array[i] = random();
    }

    return array;
}

int main(int argc, char **argv)
{
    int *array;
    int arraysize = 0;
    int seed;
    struct results r;

    /* process command line arguments */
    if (argc != 3) {
        printf("usage: ./findminmax <seed> <arraysize>\n");
        return 1;
    }

    seed = atoi(argv[1]);
    arraysize = atoi(argv[2]);
    array = generate_random_array(seed, arraysize);

    /* begin computation */

    mtf_measure_begin();

    r = find_min_and_max(array, arraysize);

    mtf_measure_end();

    printf("Execution time: ");
    mtf_measure_print_seconds(1);

    printf("min = %d, max = %d\n", r.min, r.max);

    free(array);

    return 0;
}

/* findminmax_seq.c - find the min and max values in a random array
 *
 * usage: ./findminmax <seed> <arraysize>
 *
 */
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include"measure.h"

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

int main(int argc, char **argv)
{
    int *array;
    int arraysize = 0;
    int seed;
    char randomstate[8];
    struct results r;
    int i;

    /* process command line arguments */
    if (argc != 3) {
        printf("usage: ./findminmax <seed> <arraysize>\n");
        return 1;
    }

    seed = atoi(argv[1]);
    arraysize = atoi(argv[2]);
    
    /* allocate array and populate with random values */
    array = (int *) malloc(sizeof(int) * arraysize);
    
    initstate(seed, randomstate, 8);

    for (i = 0; i < arraysize; i++) {
        array[i] = random();
    }
    
    /* begin computation */

    mtf_measure_begin();

    r = find_min_and_max(array, arraysize);
    
    mtf_measure_end();
    
    printf("Execution time: ");
    mtf_measure_print_seconds(1);

    printf("min = %d, max = %d\n", r.min, r.max);

    return 0;
}
/* findminmax_par1.c - find the min and max values in a random array
 * Comunication via files.
 *
 * usage: ./findminmax <seed> <arraysize> <nprocs>
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#include "measure.h"

#ifndef NPROCS_MAX
#define NPROCS_MAX 128
#define NPROCS_MAX_LENGTH 3
#define TMP_FILE_NAME_LENGTH NPROCS_MAX_LENGTH + 4 // id.tmp
#endif

#ifndef RESULT_LENGTH_MAX
#define RESULT_LENGTH_MAX 21 // ceil(log10(2^32))*2+1 = 21 // two strings of ints and a space
#endif

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

void write_result(int id, struct results r)
{
    char filename[TMP_FILE_NAME_LENGTH];
    sprintf(filename, "%d.tmp", id);
    FILE *f = fopen(filename, "w");

    char result[RESULT_LENGTH_MAX];
    sprintf(result, "%d %d\n", r.min, r.max);
    fputs(result, f);

    fclose(f);
}

void clean_tmp_files(int nprocs)
{
    for (int i = 0; i < nprocs; ++i) {
        char filename[TMP_FILE_NAME_LENGTH];
        sprintf(filename, "%d.tmp", i);

        remove(filename);
    }
}


int main(int argc, char **argv)
{
    int *array;
    int arraysize = 0;
    int seed;
    int nprocs;
    int childPID;
    struct results r;

    // process command line arguments
    if (argc != 4) {
        printf("usage: ./findminmax <seed> <arraysize> <nprocs>\n");
        return 1;
    }
    seed = atoi(argv[1]);
    arraysize = atoi(argv[2]);
    nprocs = atoi(argv[3]);

    // Init
    array = generate_random_array(seed, arraysize);

    // begin computation
    mtf_measure_begin();

    for (int i = 0; i < nprocs; ++i) {

        childPID = fork();

        // If fork failed, exit
        if (childPID < 0) {
            printf("Fork failed, exit!\n");
            return 1;
        }

        // If it's child
        if (childPID == 0) {
            printf("DEBUG: child %d begins.\n", i);

            int span = arraysize / nprocs;
            int low = i * span;
            if (i == nprocs - 1) { // if it's the last one
                span = arraysize - low; // deal with all remains
            }

            printf("DEBUG: child %d run on array[%d] with span %d\n", i, low, span);
            r = find_min_and_max(&array[low], span);

            write_result(i, r);

            printf("DEBUG: child %d ends with result: %d, %d.\n", i, r.min, r.max);
            return 0;
        }

    }

    // Wait for all children to finish
    printf("DEBUG: waiting for children\n");
    while ((childPID = waitpid(-1, NULL, 0))) {
       if (errno == ECHILD) {
          break;
       }
    }
    printf("DEBUG: children all done\n");

    // TODO: Find the min, max of all results


    mtf_measure_end();

    // Finishing
    printf("Execution time: ");
    mtf_measure_print_seconds(1);

    printf("min = %d, max = %d\n", r.min, r.max);

    free(array);
    clean_tmp_files(nprocs);

    return 0;
}

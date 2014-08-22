/* findminmax_par2.c - find the min and max values in a random array
 * Comunication via pipes.
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

struct results read_result(int id)
{
    char filename[TMP_FILE_NAME_LENGTH];
    sprintf(filename, "%d.tmp", id);

    struct results r;
    FILE *f = fopen(filename, "r");
    fscanf(f, "%d %d", &r.min, &r.max);
    fclose(f);

    return r;
}

void clean_tmp_files(int nprocs)
{
    for (int i = 0; i < nprocs; ++i) {
        char filename[TMP_FILE_NAME_LENGTH];
        sprintf(filename, "%d.tmp", i);

        remove(filename);
    }
}

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

struct results find_min_and_max_in_all(int *min, int *max, int nprocs)
{
    struct results r = {min[0], max[0]};
    for (int i = 1; i < nprocs; ++i) {
        if (min[i] < r.min) {
            r.min = min[i];
        }
        if (max[i] > r.max) {
            r.max = max[i];
        }
    }

    // printf("DEBUG: final result: %d, %d\n", r.min, r.max);
    return r;
}


int main(int argc, char **argv)
{
    int *array;
    int arraysize = 0;
    int seed;
    int nprocs;
    int childPID;
    int fd[2]; // for pipe()
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
    int min[nprocs], max[nprocs];

    // begin computation
    mtf_measure_begin();

    for (int i = 0; i < nprocs; ++i) {

        pipe(fd);

        childPID = fork();

        // If fork failed, exit
        if (childPID < 0) {
            perror("Fork failed, exit!\n");
            exit(1);
        }

        // If it's child
        if (childPID == 0) {
            // printf("DEBUG: child %d begins.\n", i);

            int span = arraysize / nprocs;
            int low = i * span;
            if (i == nprocs - 1) { // if it's the last one
                span = arraysize - low; // deal with all remains
            }

            // printf("DEBUG: child %d run on array[%d] with span %d\n", i, low, span);
            r = find_min_and_max(&array[low], span);

            write(fd[1], &r, sizeof(r));

            close(fd[0]);
            close(fd[1]);

            // printf("DEBUG: child %d ends with result: %d, %d.\n", i, r.min, r.max);
            exit(0);
        } else { // if it's parent
            read(fd[0], &min[i], sizeof(min[i]));
            read(fd[0], &max[i], sizeof(max[i]));

            close(fd[0]);
            close(fd[1]);
        }

    }

    // Wait for all children to finish
    // printf("DEBUG: waiting for children\n");
    while ((childPID = waitpid(-1, NULL, 0))) {
       if (errno == ECHILD) {
          break;
       }
    }
    // printf("DEBUG: children all done\n");

    // Find the min, max of all results
    r = find_min_and_max_in_all(min, max, nprocs);

    mtf_measure_end();

    // Finishing
    printf("Execution time: ");
    mtf_measure_print_seconds(1);

    printf("min = %d, max = %d\n", r.min, r.max);

    free(array);
    clean_tmp_files(nprocs);

    return 0;
}

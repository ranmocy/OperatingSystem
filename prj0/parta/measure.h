/* measure.h - macros for timing code */

#include <sys/time.h>
#include <unistd.h>

/* global variables for collecting times */

struct timeval mtf_measure_begin, mtf_measure_end;
int    mtf_measure_ubegin, mtf_measure_uend;
double mtf_measure_diff, mtf_measure_tpo;


#define mtf_measure_begin() gettimeofday(&mtf_measure_begin, (struct timezone *) NULL)
#define mtf_measure_end() gettimeofday(&mtf_measure_end, (struct timezone *) NULL)

#define mtf_measure_print(div) \
        mtf_measure_ubegin = (1000000 * mtf_measure_begin.tv_sec)    \
                             + mtf_measure_begin.tv_usec;            \
        mtf_measure_uend   = (1000000 * mtf_measure_end.tv_sec)      \
                             + mtf_measure_end.tv_usec;              \
        mtf_measure_diff   = mtf_measure_uend - mtf_measure_ubegin;  \
        mtf_measure_tpo    = mtf_measure_diff / div;                \
        printf("%.4f\n", mtf_measure_tpo);

#define mtf_measure_print_seconds(div) \
        mtf_measure_ubegin = (1000000 * mtf_measure_begin.tv_sec)    \
                             + mtf_measure_begin.tv_usec;            \
        mtf_measure_uend   = (1000000 * mtf_measure_end.tv_sec)      \
                             + mtf_measure_end.tv_usec;              \
        mtf_measure_diff   = mtf_measure_uend - mtf_measure_ubegin;  \
        mtf_measure_tpo    = (mtf_measure_diff / 1000000) / div;     \
        printf("%.4f\n", mtf_measure_tpo);

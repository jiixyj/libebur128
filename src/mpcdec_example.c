/* See LICENSE file for copyright and license details. */
#define _POSIX_C_SOURCE 200112L
#include <float.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


#include "./ebur128.h"

#define CHECK_ERROR(condition, message, errorcode, goto_point)                 \
  if ((condition)) {                                                           \
    fprintf(stderr, message);                                                  \
    errcode = (errorcode);                                                     \
    goto goto_point;                                                           \
  }


int main(int ac, char* const av[]) {
  double gated_loudness = DBL_MAX;
  double* segment_loudness;
  double* segment_peaks;
  int calculate_lra = 0;
  int rgtag_info = 0;


  CHECK_ERROR(ac < 2, "usage: r128-test [-r] [-t] FILENAME(S) ...\n\n"
                      " -r: calculate loudness range in LRA\n"
                      " -t: output ReplayGain tagging info\n", 1, exit)
  while ((c = getopt(ac, av, "tr")) != -1) {
    switch (c) {
      case 't':
        rgtag_info = 1;
        break;
      case 'r':
        calculate_lra = 1;
        break;
      default:
        return 1;
        break;
    }
  }

  segment_loudness = calloc((size_t) (ac - optind), sizeof(double));
  segment_peaks = calloc((size_t) (ac - optind), sizeof(double));

  for (i = optind; i < ac; ++i) {
  }

  if (st && calculate_lra) {
    fprintf(stderr, "LRA: %.2f\n", ebur128_loudness_range(st));
  }

  if (st && rgtag_info) {
    double global_peak = 0.0;
    for (i = 0; i < ac - optind; ++i) {
      if (segment_peaks[i] > global_peak) {
        global_peak = segment_peaks[i];
      }
    }
    for (i = optind; i < ac; ++i) {
      printf("%.8f %.8f %.8f %.8f\n", -18.0 - segment_loudness[i - optind],
                                      segment_peaks[i - optind],
                                      -18.0 - gated_loudness,
                                      global_peak);
    }
  }

  if (st)
    ebur128_destroy(&st);
  if (segment_loudness)
    free(segment_loudness);
  if (segment_peaks)
    free(segment_peaks);

exit:
  return errcode;
}

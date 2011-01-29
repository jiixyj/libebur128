/* See LICENSE file for copyright and license details. */
#define _POSIX_C_SOURCE 200112L
#include <float.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <glib.h>

#include "./ebur128.h"
#include "./common.h"

extern long nproc();

extern int init_input_library();
extern void exit_input_library();
extern void calculate_gain_of_file(void* user, void* user_data);

int main(int ac, char* const av[]) {
  int result;
  int i, c;
  int errcode;

  GThreadPool* pool;
  struct gain_data gd;
  gd.calculate_lra = 0;
  gd.tag_rg = 0;

  g_thread_init(NULL);

  CHECK_ERROR(ac < 2, "usage: r128-test [-r] [-t] FILENAME(S) ...\n\n"
                      " -r: calculate loudness range in LRA\n"
                      " -t: output ReplayGain tagging info\n", 1, exit)
  while ((c = getopt(ac, av, "tr")) != -1) {
    switch (c) {
      case 't':
        gd.tag_rg = 1;
        break;
      case 'r':
        gd.calculate_lra = 1;
        break;
      default:
        return 1;
        break;
    }
  }

  CHECK_ERROR(init_input_library(),
              "Could not initialize input library!", 1, exit)

  gd.file_names = &av[optind];
  gd.segment_loudness = calloc((size_t) (ac - optind), sizeof(double));
  gd.segment_peaks = calloc((size_t) (ac - optind), sizeof(double));
  gd.library_states = calloc((size_t) (ac - optind), sizeof(ebur128_state*));

  pool = g_thread_pool_new(calculate_gain_of_file, &gd, (int) nproc(),
                           FALSE, NULL);

  for (i = optind; i < ac; ++i) {
    g_thread_pool_push(pool, GINT_TO_POINTER(i - optind + 1), NULL);
  }
  g_thread_pool_free(pool, FALSE, TRUE);
  for (i = optind; i < ac; ++i) {
    fprintf(stderr, "\r");
    fprintf(stderr, "segment %d: %.2f LUFS\n", (int) i - optind + 1,
                    gd.segment_loudness[i - optind]);
  }

  result = 1;
  for (i = 0; i < ac - optind; ++i) {
    if (!gd.library_states[i]) {
      result = 0;
    }
  }

  if (result) {
    double gated_loudness;
    gated_loudness = ebur128_loudness_global_multiple(gd.library_states,
                                                      (size_t) (ac - optind));
    fprintf(stderr, "global loudness: %.2f LUFS\n", gated_loudness);


    if (gd.calculate_lra) {
      fprintf(stderr, "LRA: %.2f\n", ebur128_loudness_range_multiple
                                                     (gd.library_states,
                                                      (size_t) (ac - optind)));
    }


    if (gd.tag_rg) {
      double global_peak = 0.0;
      for (i = 0; i < ac - optind; ++i) {
        if (gd.segment_peaks[i] > global_peak) {
          global_peak = gd.segment_peaks[i];
        }
      }
      for (i = optind; i < ac; ++i) {
        printf("%.8f %.8f %.8f %.8f\n", -18.0 - gd.segment_loudness[i - optind],
                                        gd.segment_peaks[i - optind],
                                        -18.0 - gated_loudness,
                                        global_peak);
      }
    }
  }

  for (i = 0; i < ac - optind; ++i) {
    if (gd.library_states[i]) {
      ebur128_destroy(&gd.library_states[i]);
    }
  }
  free(gd.library_states);
  free(gd.segment_loudness);
  free(gd.segment_peaks);
  exit_input_library();

exit:
  return errcode;
}

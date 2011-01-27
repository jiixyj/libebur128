/* See LICENSE file for copyright and license details. */
#define _POSIX_C_SOURCE 200112L
#include <float.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <mpg123.h>

#include "./ebur128.h"

#define CHECK_ERROR(condition, message, errorcode, goto_point)                 \
  if ((condition)) {                                                           \
    fprintf(stderr, message);                                                  \
    errcode = (errorcode);                                                     \
    goto goto_point;                                                           \
  }


int main(int ac, char* const av[]) {
  mpg123_handle* mh = NULL;
  long mh_rate;
  int mh_channels, mh_encoding;

  int channels, samplerate;
  size_t nr_frames_read, nr_frames_read_all;

  float* buffer;

  ebur128_state* st = NULL;
  double gated_loudness = DBL_MAX;
  double* segment_loudness;
  double* segment_peaks;
  int calculate_lra = 0;
  int rgtag_info = 0;

  int errcode = 0;
  int result;
  int i;
  int c;

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

  result = mpg123_init();
  if (result != MPG123_OK) {
    fprintf(stderr, "Could not initialize mpg123!");
    return 1;
  }

  segment_loudness = calloc((size_t) (ac - optind), sizeof(double));
  segment_peaks = calloc((size_t) (ac - optind), sizeof(double));
  for (i = optind; i < ac; ++i) {
    segment_loudness[i - optind] = DBL_MAX;
    mh = mpg123_new(NULL, &result);
    CHECK_ERROR(!mh, "Could not create mpg123 handler!\n", 1, close_file)
    result = mpg123_open(mh, av[i]);
    CHECK_ERROR(result != MPG123_OK, "Could not open input file!\n", 1,
                                     close_file)
    result = mpg123_getformat(mh, &mh_rate, &mh_channels, &mh_encoding);
    CHECK_ERROR(result != MPG123_OK, "mpg123_getformat failed!\n", 1,
                                     close_file)
    result = mpg123_format_none(mh);
    CHECK_ERROR(result != MPG123_OK, "mpg123_format_none failed!\n", 1,
                                     close_file)
    result = mpg123_format(mh, mh_rate, mh_channels, MPG123_ENC_FLOAT_32);
    CHECK_ERROR(result != MPG123_OK, "mpg123_format failed!\n", 1, close_file)
    result = mpg123_close(mh);
    result = mpg123_open(mh, av[i]);
    CHECK_ERROR(result != MPG123_OK, "Could not open input file!\n", 1,
                                     close_file)
    result = mpg123_getformat(mh, &mh_rate, &mh_channels, &mh_encoding);
    CHECK_ERROR(result != MPG123_OK, "mpg123_getformat failed!\n", 1,
                                     close_file)

    channels = mh_channels;
    samplerate = (int) mh_rate;
    nr_frames_read_all = 0;

    if (!st) {
      st = ebur128_init(channels,
                        samplerate,
                        EBUR128_MODE_I |
                        (calculate_lra ? EBUR128_MODE_LRA : 0));
      CHECK_ERROR(!st, "Could not initialize EBU R128!\n", 1, close_file)
    } else {
      CHECK_ERROR(st->channels != (size_t) channels ||
                  st->samplerate != (size_t) samplerate,
                  "All files must have the same samplerate "
                  "and number of channels! Skipping...\n",
                  1, close_file)
    }

    buffer = (float*) malloc(st->samplerate * st->channels * sizeof(float));
    CHECK_ERROR(!buffer, "Could not allocate memory!\n", 1, close_file)
    segment_peaks[i - optind] = 0.0;
    for (;;) {
      result = mpg123_read(mh, (unsigned char*) buffer,
                               st->samplerate * st->channels * sizeof(float),
                               &nr_frames_read);
      if (result != MPG123_OK && result != MPG123_DONE) {
        if (result == MPG123_ERR && mpg123_errcode(mh) == MPG123_RESYNC_FAIL) {
          fprintf(stderr, "%s\n", mpg123_strerror(mh));
          fprintf(stderr, "Maybe your file has an APEv2 tag?\n");
          break;
        } else {
          fprintf(stderr, "Internal MPG123 error!\n");
          errcode = 1;
          goto free_buffer;
        }
      }
      nr_frames_read /= st->channels * sizeof(float);
      if (!nr_frames_read) break;
      if (rgtag_info) {
        size_t j;
        for (j = 0; j < (size_t) nr_frames_read * st->channels; ++j) {
          if (buffer[j] > segment_peaks[i - optind])
            segment_peaks[i - optind] = buffer[j];
          else if (-buffer[j] > segment_peaks[i - optind])
            segment_peaks[i - optind] = -buffer[j];
        }
      }
      nr_frames_read_all += nr_frames_read;
      result = ebur128_add_frames_float(st, buffer, (size_t) nr_frames_read);
      CHECK_ERROR(result, "Internal EBU R128 error!\n", 1, free_buffer)
    }

    segment_loudness[i - optind] = ebur128_loudness_segment(st);
    if (ac - optind != 1) {
      fprintf(stderr, "segment %d: %.2f LUFS\n", i + 1 - optind,
                      segment_loudness[i - optind]);
      ebur128_start_new_segment(st);
    }
    if (i == ac - 1) {
      gated_loudness = ebur128_loudness_global(st);
      fprintf(stderr, "global loudness: %.2f LUFS\n", gated_loudness);
    }

  free_buffer:
    free(buffer);
    buffer = NULL;

  close_file:
    mpg123_close(mh);
    mpg123_delete(mh);
    mh = NULL;
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

  mpg123_exit();

exit:
  return errcode;
}

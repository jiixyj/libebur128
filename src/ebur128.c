#include "./ebur128.h"

#include <stdio.h>

#define CHECK_ERROR(condition, message, errorcode, goto_point)                 \
  if ((condition)) {                                                           \
    fprintf(stderr, message);                                                  \
    errcode = (errorcode);                                                     \
    goto goto_point;                                                           \
  }

int filter(double* dest, const double* source,
           size_t frames, int channels, int c,
           const double* b,
           const double* a,
           double** v) {
  size_t i;
  for (i = 0; i < frames; ++i) {
    v[c][0] = source[i * (size_t) channels + (size_t) c]
                - a[1] * v[c][1]
                - a[2] * v[c][2];
    dest[i * (size_t) channels + (size_t) c] =
                  b[0] * v[c][0]
                + b[1] * v[c][1]
                + b[2] * v[c][2];
    memmove(&v[c][1], &v[c][0], 2 * sizeof(double));
  }
  return 0;
}

int do_stuff(double* audio_data, size_t frames, int channels,
             double** v, double** v2,
             double* z) {

  static double b[] = {1.53512485958697, -2.69169618940638, 1.19839281085285};
  static double a[] = {1.0, -1.69065929318241, 0.73248077421585};
  static double b2[] = {1.0, -2.0, 1.0};
  static double a2[] = {1.0, -1.99004745483398, 0.99007225036621};
  int c;
  size_t i;
  double tmp;
  for (c = 0; c < channels; ++c) {
    filter(audio_data, audio_data,
           frames, channels, c,
           b, a,
           v);
    filter(audio_data, audio_data,
           frames, channels, c,
           b2, a2,
           v2);
    tmp = 0.0;
    for (i = 0; i < frames; ++i) {
      tmp += audio_data[i * (size_t) channels + (size_t) c] *
             audio_data[i * (size_t) channels + (size_t) c];
    }
    z[c] += tmp;
  }

  return 0;
}

int init_filter_state(double*** v, int channels, int filter_size) {
  int i, errcode = 0;
  *v = (double**) calloc((size_t) channels, sizeof(double*));
  CHECK_ERROR(!(*v), "Could not allocate memory!\n", 1, exit)
  for (i = 0; i < channels; ++i) {
    (*v)[i] = (double*) calloc((size_t) filter_size, sizeof(double));
    CHECK_ERROR(!((*v)[i]), "Could not allocate memory!\n", 1, free_all)
  }
  return 0;

free_all:
  for (i = 0; i < channels; ++i) {
    free((*v)[i]);
  }
  free(*v);
exit:
  return errcode;
}

void release_filter_state(double*** v, int channels) {
  int i;
  for (i = 0; i < channels; ++i) {
    free((*v)[i]);
  }
  free(*v);
}

void calc_gating_block(double* audio_data, size_t nr_frames_read,
                       int channels,
                       double** zg, size_t zg_index) {
  int i, c;
  for (c = 0; c < channels; ++c) {
    double sum = 0.0;
    for (i = 0; i < nr_frames_read; ++i) {
      sum += audio_data[i * (size_t) channels + (size_t) c] *
             audio_data[i * (size_t) channels + (size_t) c];
    }
    sum /= nr_frames_read;
    zg[c][zg_index] = sum;
  }
  return;
}

void calculate_block_loudness(double* lg, double** zg,
                              size_t frames, int channels) {
  int i;
  for (i = 0; i < frames / 9600 - 1; ++i) {
    int j;
    for (j = 0; j < channels; ++j) {
      switch (j) {
        case 0: case 1: case 2:
          break;
        case 4: case 5:
          zg[j][i] *= 1.41;
          break;
        default:
          zg[j][i] *= 0;
      }
      lg[i] += zg[j][i];
    }
    lg[i] = 10 * (log(lg[i]) / log(10.0));
    lg[i] -= 0.691;
  }
}

void calculate_relative_threshold(double* relative_threshold,
                                  double* lg, double** zg,
                                  size_t frames, int channels) {
  int i, j;
  *relative_threshold = 0.0;
  for (j = 0; j < channels; ++j) {
    double tmp = 0.0;
    int above_thresh_counter = 0;
    for (i = 0; i < frames / 9600 - 1; ++i) {
      if (lg[i] >= -70) {
        ++above_thresh_counter;
        tmp += zg[j][i];
      }
    }
    tmp /= above_thresh_counter;
    *relative_threshold += tmp;
  }
  *relative_threshold = 10 * (log(*relative_threshold) / log(10.0));
  *relative_threshold -= 0.691;
  *relative_threshold -= 8.0;
}

void calculate_gated_loudness(double* gated_loudness,
                              double relative_threshold,
                              double* lg, double** zg,
                              size_t frames, int channels) {
  int i, j;
  *gated_loudness = 0.0;
  for (j = 0; j < channels; ++j) {
    double tmp = 0.0;
    int above_thresh_counter = 0;
    for (i = 0; i < frames / 9600 - 1; ++i) {
      if (lg[i] >= relative_threshold) {
        ++above_thresh_counter;
        tmp += zg[j][i];
      }
    }
    tmp /= above_thresh_counter;
    *gated_loudness += tmp;
  }
  *gated_loudness = 10 * (log(*gated_loudness) / log(10.0));
  *gated_loudness -= 0.691;
}

#include "./ebur128.h"

#include <stdio.h>

#define CHECK_ERROR(condition, message, errorcode, goto_point)                 \
  if ((condition)) {                                                           \
    fprintf(stderr, message);                                                  \
    errcode = (errorcode);                                                     \
    goto goto_point;                                                           \
  }

int ebur128_init(ebur128_state* state, size_t frames, int channels) {
  state->audio_data = (double*) malloc((size_t) 19200
                                     * (size_t) channels
                                     * sizeof(double));
  state->audio_data_index = 0;
  state->frames = frames;
  state->channels = channels;
  state->audio_data_half = 0;
  init_filter_state(&(state->v), channels, 3);
  init_filter_state(&(state->v2), channels, 3);
  state->z = (double*) calloc((size_t) channels, sizeof(double));
  init_filter_state(&(state->zg), channels, frames / 9600 - 1);
  state->zg_index = 0;
  double loudness = 0.0;
  state->lg = (double*) calloc((size_t) frames / 9600 - 1, sizeof(double));

  return 0;
}

int ebur128_write_frames(ebur128_state* st,
                         const double* src, size_t frames) {
  size_t src_index = 0;
  while (frames > 0) {
    size_t needed_frames = 19200 - st->audio_data_index / st->channels;
    if (frames >= needed_frames) {
      memcpy(&st->audio_data[st->audio_data_index], &src[src_index], needed_frames * st->channels * sizeof(double));
      src_index += needed_frames * st->channels;
      frames -= needed_frames;
      ebur128_do_stuff(st, needed_frames);
      calc_gating_block(st->audio_data, 19200, st->channels, st->zg, st->zg_index);
      ++st->zg_index;
      memcpy(st->audio_data, st->audio_data + 9600 * st->channels, 9600 * st->channels * sizeof(double));
      st->audio_data_index = 9600 * st->channels;
    } else {
      memcpy(&st->audio_data[st->audio_data_index], &src[src_index], frames * st->channels * sizeof(double));
      ebur128_do_stuff(st, frames);
      st->audio_data_index += frames * st->channels;
      frames = 0;
    }
  }
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

int ebur128_do_stuff(ebur128_state* st, size_t frames) {

  static double b[] = {1.53512485958697, -2.69169618940638, 1.19839281085285};
  static double a[] = {1.0, -1.69065929318241, 0.73248077421585};
  static double b2[] = {1.0, -2.0, 1.0};
  static double a2[] = {1.0, -1.99004745483398, 0.99007225036621};
  int c;
  size_t i;
  double tmp;
  double* audio_data = st->audio_data + st->audio_data_index;
  for (c = 0; c < st->channels; ++c) {
    filter(audio_data, audio_data,
           frames, st->channels, c,
           b, a,
           st->v);
    filter(audio_data, audio_data,
           frames, st->channels, c,
           b2, a2,
           st->v2);
    tmp = 0.0;
    for (i = 0; i < frames; ++i) {
      tmp += audio_data[i * (size_t) st->channels + (size_t) c] *
             audio_data[i * (size_t) st->channels + (size_t) c];
    }
    st->z[c] += tmp;
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

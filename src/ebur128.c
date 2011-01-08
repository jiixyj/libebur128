#include "./ebur128.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK_ERROR(condition, message, errorcode, goto_point)                 \
  if ((condition)) {                                                           \
    fprintf(stderr, message);                                                  \
    errcode = (errorcode);                                                     \
    goto goto_point;                                                           \
  }

int ebur128_init_multi_array(double*** v, size_t channels, size_t filter_size) {
  size_t i;
  int errcode = 0;
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

ebur128_state* ebur128_init(size_t frames, int channels) {
  ebur128_state* state = (ebur128_state*) malloc(sizeof(ebur128_state));
  state->audio_data = (double*) malloc((size_t) 19200
                                     * (size_t) channels
                                     * sizeof(double));
  state->audio_data_index = 0;
  state->frames = frames;
  if (frames / 9600 - 1 < 1) return NULL;
  state->blocks = (size_t) (frames / 9600 - 1);
  state->channels = (size_t) channels;
  ebur128_init_multi_array(&(state->v), state->channels, 3);
  ebur128_init_multi_array(&(state->v2), state->channels, 3);
  state->z = (double*) calloc(state->channels, sizeof(double));
  ebur128_init_multi_array(&(state->zg), state->channels, state->blocks);
  state->zg_index = 0;
  state->lg = (double*) calloc(state->blocks, sizeof(double));

  return state;
}

void ebur128_release_multi_array(double*** v, size_t channels) {
  size_t i;
  for (i = 0; i < channels; ++i) {
    free((*v)[i]);
  }
  free(*v);
}

int ebur128_destroy(ebur128_state** st) {
  free((*st)->audio_data);
  ebur128_release_multi_array(&(*st)->v, (*st)->channels);
  ebur128_release_multi_array(&(*st)->v2, (*st)->channels);
  free((*st)->z);
  ebur128_release_multi_array(&(*st)->zg, (*st)->channels);
  free((*st)->lg);

  free(*st);
  *st = NULL;

  return 0;
}

int ebur128_filter(double* dest, const double* source,
                   size_t frames, size_t channels, size_t c,
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

int ebur128_filter_new_frames(ebur128_state* st, size_t frames) {

  static double b[] = {1.53512485958697, -2.69169618940638, 1.19839281085285};
  static double a[] = {1.0, -1.69065929318241, 0.73248077421585};
  static double b2[] = {1.0, -2.0, 1.0};
  static double a2[] = {1.0, -1.99004745483398, 0.99007225036621};
  size_t c;
  size_t i;
  double tmp;
  double* audio_data = st->audio_data + st->audio_data_index;
  for (c = 0; c < st->channels; ++c) {
    ebur128_filter(audio_data, audio_data,
                   frames, st->channels, c,
                   b, a,
                   st->v);
    ebur128_filter(audio_data, audio_data,
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

void ebur128_calc_gating_block(ebur128_state* st) {
  size_t i, c;
  for (c = 0; c < st->channels; ++c) {
    double sum = 0.0;
    for (i = 0; i < 19200; ++i) {
      sum += st->audio_data[i * (size_t) st->channels + (size_t) c] *
             st->audio_data[i * (size_t) st->channels + (size_t) c];
    }
    sum /= 19200;
    st->zg[c][st->zg_index] = sum;
  }
}

void ebur128_calc_block_loudness(ebur128_state* st) {
  size_t c;
  for (c = 0; c < st->channels; ++c) {
    switch (c) {
      case 0: case 1: case 2:
        break;
      case 4: case 5:
        st->zg[c][st->zg_index] *= 1.41;
        break;
      default:
        st->zg[c][st->zg_index] *= 0;
    }
    st->lg[st->zg_index] += st->zg[c][st->zg_index];
  }
  st->lg[st->zg_index] = 10 * (log(st->lg[st->zg_index]) / log(10.0));
  st->lg[st->zg_index] -= 0.691;
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
      ebur128_filter_new_frames(st, needed_frames);
      if (st->zg_index >= st->blocks) return 1;
      ebur128_calc_gating_block(st);
      ebur128_calc_block_loudness(st);
      ++st->zg_index;
      memcpy(st->audio_data, st->audio_data + 9600 * st->channels, 9600 * st->channels * sizeof(double));
      st->audio_data_index = 9600 * st->channels;
    } else {
      memcpy(&st->audio_data[st->audio_data_index], &src[src_index], frames * st->channels * sizeof(double));
      ebur128_filter_new_frames(st, frames);
      st->audio_data_index += frames * st->channels;
      frames = 0;
    }
  }
  return 0;
}

double ebur128_relative_threshold(ebur128_state* st) {
  size_t i, j;
  double relative_threshold = 0.0;
  for (j = 0; j < st->channels; ++j) {
    double tmp = 0.0;
    int above_thresh_counter = 0;
    for (i = 0; i < st->zg_index; ++i) {
      if (st->lg[i] >= -70) {
        ++above_thresh_counter;
        tmp += st->zg[j][i];
      }
    }
    tmp /= above_thresh_counter;
    relative_threshold += tmp;
  }
  relative_threshold = 10 * (log(relative_threshold) / log(10.0));
  relative_threshold -= 0.691;
  relative_threshold -= 8.0;
  return relative_threshold;
}

double ebur128_gated_loudness(ebur128_state* st, double relative_threshold) {
  size_t i, j;
  double gated_loudness = 0.0;
  for (j = 0; j < st->channels; ++j) {
    double tmp = 0.0;
    int above_thresh_counter = 0;
    for (i = 0; i < st->zg_index; ++i) {
      if (st->lg[i] >= relative_threshold) {
        ++above_thresh_counter;
        tmp += st->zg[j][i];
      }
    }
    tmp /= above_thresh_counter;
    gated_loudness += tmp;
  }
  gated_loudness = 10 * (log(gated_loudness) / log(10.0));
  gated_loudness -= 0.691;
  return gated_loudness;
}

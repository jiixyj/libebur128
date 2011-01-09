#include "./ebur128.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define PI 3.14159265358979323846

#define CHECK_ERROR(condition, message, errorcode, goto_point)                 \
  if ((condition)) {                                                           \
    fprintf(stderr, message);                                                  \
    errcode = (errorcode);                                                     \
    goto goto_point;                                                           \
  }

int ebur128_init_multi_array(double*** v, size_t channels, size_t filter_size) {
  size_t i;
  int errcode = 0;
  *v = (double**) calloc(channels, sizeof(double*));
  CHECK_ERROR(!(*v), "Could not allocate memory!\n", 1, exit)
  for (i = 0; i < channels; ++i) {
    (*v)[i] = (double*) calloc(filter_size, sizeof(double));
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

void ebur128_release_multi_array(double*** v, size_t channels) {
  size_t i;
  for (i = 0; i < channels; ++i) {
    free((*v)[i]);
  }
  free(*v);
  *v = NULL;
}

int ebur128_init_filter(ebur128_state* st) {
  int errcode = 0;
  double fc = 1681.974450955531;
  double G = 3.999843853973343;
  double K = tan(PI * fc / st->samplerate);
  double v0 = pow(10, G / 20.0);

  double sqrt2 = pow(2, 0.5);   /* BS.1770-1 uses 1.414076664088622 */
  double sqrtv0 = pow(v0, 0.5); /* BS.1770-1 uses 1.258720930232561 */

  double b1[] = {0.0, 0.0, 0.0};
  double a1[] = {1.0, 0.0, 0.0};
  double denom = 1 + sqrt2 * K + K * K;
  b1[0] = (v0 + sqrt2 * sqrtv0 * K + K * K) / denom;
  b1[1] = 2 * (K * K - v0) / denom;
  b1[2] = (v0 - sqrt2 * sqrtv0 * K + K * K) / denom;
  a1[1] = 2 * (K * K - 1) / denom;
  a1[2] = (1 - sqrt2 * K + K * K) / denom;


  double f0 = 38.13547087606643;
  double Q = 0.500327037324428;

  double w0 = 2 * PI * f0 / (double) st->samplerate;
  double alpha = sin(w0) / (2*Q);

  double b2[] = {1.0, -2.0, 1.0};
  double a2[] = {1.0, 0.0, 0.0};
  a2[1] = -2 * cos(w0) / (1.0 + alpha);
  a2[2] = (1.0 - alpha) / (1.0 + alpha);


  st->a = (double*) calloc(5, sizeof(double));
  CHECK_ERROR(errcode, "Could not allocate memory!\n", 1, exit)
  st->b = (double*) calloc(5, sizeof(double));
  CHECK_ERROR(errcode, "Could not allocate memory!\n", 1, free_a)

  st->b[0] = b1[0] * b2[0];
  st->b[1] = b1[0] * b2[1] + b1[1] * b2[0];
  st->b[2] = b1[0] * b2[2] + b1[1] * b2[1] + b1[2] * b2[0];
  st->b[3] = b1[1] * b2[2] + b1[2] * b2[1];
  st->b[4] = b1[2] * b2[2];

  st->a[0] = a1[0] * a2[0];
  st->a[1] = a1[0] * a2[1] + a1[1] * a2[0];
  st->a[2] = a1[0] * a2[2] + a1[1] * a2[1] + a1[2] * a2[0];
  st->a[3] = a1[1] * a2[2] + a1[2] * a2[1];
  st->a[4] = a1[2] * a2[2];

  return 0;

free_a:
  free(st->a);
exit:
  return errcode;
}

ebur128_state* ebur128_init(size_t frames, int channels, int samplerate) {
  int errcode;
  ebur128_state* state;

  if (frames / (samplerate / 5) - 1 < 1) return NULL;
  state = (ebur128_state*) malloc(sizeof(ebur128_state));
  CHECK_ERROR(!state, "Could not allocate memory!\n", 0, exit)
  state->channels = (size_t) channels;
  state->samplerate = (size_t) samplerate;
  state->audio_data = (double*) malloc(state->samplerate * 2 / 5
                                     * state->channels
                                     * sizeof(double));
  CHECK_ERROR(!state->audio_data, "Could not allocate memory!\n", 0, free_state)
  state->audio_data_index = 0;
  state->frames = frames;
  state->blocks = (size_t) (frames / (state->samplerate / 5) - 1);
  errcode = ebur128_init_multi_array(&(state->v), state->channels, 5);
  CHECK_ERROR(errcode, "Could not allocate memory!\n", 0, free_audio_data)
  errcode = ebur128_init_multi_array(&(state->zg), state->channels, state->blocks);
  CHECK_ERROR(errcode, "Could not allocate memory!\n", 0, free_v)
  state->zg_index = 0;
  state->lg = (double*) calloc(state->blocks, sizeof(double));
  CHECK_ERROR(!state->lg, "Could not allocate memory!\n", 0, free_zg)
  errcode = ebur128_init_filter(state);
  CHECK_ERROR(errcode, "Could not initialize filter!\n", 0, free_lg)

  return state;

free_lg:
  free(state->lg);
free_zg:
  ebur128_release_multi_array(&(state->zg), state->channels);
free_v:
  ebur128_release_multi_array(&(state->v), state->channels);
free_audio_data:
  free(state->audio_data);
free_state:
  free(state);
exit:
  return NULL;
}

int ebur128_destroy(ebur128_state** st) {
  free((*st)->audio_data);
  ebur128_release_multi_array(&(*st)->v, (*st)->channels);
  ebur128_release_multi_array(&(*st)->zg, (*st)->channels);
  free((*st)->a);
  free((*st)->b);
  free((*st)->lg);

  free(*st);
  *st = NULL;

  return 0;
}

int ebur128_filter(ebur128_state* st, size_t frames) {
  double* audio_data = st->audio_data + st->audio_data_index;
  size_t i, c;
  for (c = 0; c < st->channels; ++c) {
    for (i = 0; i < frames; ++i) {
      st->v[c][0] = audio_data[i * st->channels + c]
                  - st->a[1] * st->v[c][1]
                  - st->a[2] * st->v[c][2]
                  - st->a[3] * st->v[c][3]
                  - st->a[4] * st->v[c][4];
      audio_data[i * st->channels + c] =
                    st->b[0] * st->v[c][0]
                  + st->b[1] * st->v[c][1]
                  + st->b[2] * st->v[c][2]
                  + st->b[3] * st->v[c][3]
                  + st->b[4] * st->v[c][4];
      st->v[c][4] = st->v[c][3];
      st->v[c][3] = st->v[c][2];
      st->v[c][2] = st->v[c][1];
      st->v[c][1] = st->v[c][0];
    }
  }
  return 0;
}

void ebur128_calc_gating_block(ebur128_state* st) {
  size_t i, c;
  for (c = 0; c < st->channels; ++c) {
    double sum = 0.0;
    for (i = 0; i < st->samplerate * 2 / 5; ++i) {
      sum += st->audio_data[i * st->channels + c] *
             st->audio_data[i * st->channels + c];
    }
    sum /= st->samplerate * 2 / 5;
    st->zg[c][st->zg_index] = sum;
  }
}

void ebur128_calc_block_loudness(ebur128_state* st) {
  size_t c;
  for (c = 0; c < st->channels; ++c) {
    if (c == 0 || c == 1 || c == 2) {
    } else if (st->channels == 5 && (c == 3 || c == 4)) {
      st->zg[c][st->zg_index] *= 1.41;
    } else if (st->channels == 6 && (c == 4 || c == 5)) {
      st->zg[c][st->zg_index] *= 1.41;
    } else {
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
    size_t needed_frames = st->samplerate * 2 / 5 - st->audio_data_index / st->channels;
    if (frames >= needed_frames) {
      memcpy(&st->audio_data[st->audio_data_index], &src[src_index], needed_frames * st->channels * sizeof(double));
      src_index += needed_frames * st->channels;
      frames -= needed_frames;
      ebur128_filter(st, needed_frames);
      if (st->zg_index >= st->blocks) return 1;
      ebur128_calc_gating_block(st);
      ebur128_calc_block_loudness(st);
      ++st->zg_index;
      memcpy(st->audio_data, st->audio_data + st->samplerate / 5 * st->channels, st->samplerate / 5 * st->channels * sizeof(double));
      st->audio_data_index = st->samplerate / 5 * st->channels;
    } else {
      memcpy(&st->audio_data[st->audio_data_index], &src[src_index], frames * st->channels * sizeof(double));
      ebur128_filter(st, frames);
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

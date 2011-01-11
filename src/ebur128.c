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
  double K = tan(PI * fc / (double) st->samplerate);
  double v0 = pow(10, G / 20.0);

  double sqrt2 = 1.414076664088622;
  double sqrtv0 = st->samplerate == 48000 ? 1.258720930232561 : pow(v0, 0.5);

  double b1[] = {0.0, 0.0, 0.0};
  double a1[] = {1.0, 0.0, 0.0};
  double denom = 1 + sqrt2 * K + K * K;
  b1[0] = (v0 + sqrt2 * sqrtv0 * K + K * K) / denom;
  b1[1] = 2 * (K * K - v0) / denom;
  b1[2] = (v0 - sqrt2 * sqrtv0 * K + K * K) / denom;
  a1[1] = 2 * (K * K - 1) / denom;
  a1[2] = (1 - sqrt2 * K + K * K) / denom;

  {
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
  }

  return 0;

free_a:
  free(st->a);
exit:
  return errcode;
}

int ebur128_init_channel_map(ebur128_state* st) {
  size_t i;
  st->channel_map = (int*) calloc(st->channels, sizeof(int));
  if (!st->channel_map) return 1;
  for (i = 0; i < st->channels; ++i) {
    switch (i) {
      case 0:  st->channel_map[i] = EBUR128_LEFT;           break;
      case 1:  st->channel_map[i] = EBUR128_RIGHT;          break;
      case 2:  st->channel_map[i] = EBUR128_CENTER;         break;
      case 3:  st->channel_map[i] = EBUR128_UNUSED;         break;
      case 4:  st->channel_map[i] = EBUR128_LEFT_SURROUND;  break;
      case 5:  st->channel_map[i] = EBUR128_RIGHT_SURROUND; break;
      default: st->channel_map[i] = EBUR128_UNUSED;         break;
    }
  }
  return 0;
}

ebur128_state* ebur128_init(int channels, int samplerate) {
  int errcode;
  ebur128_state* state;

  state = (ebur128_state*) malloc(sizeof(ebur128_state));
  CHECK_ERROR(!state, "Could not allocate memory!\n", 0, exit)
  state->channels = (size_t) channels;
  errcode = ebur128_init_channel_map(state);
  CHECK_ERROR(errcode, "Could not initialize channel map!\n", 0, free_state)
  state->samplerate = (size_t) samplerate;
  state->audio_data = (double*) malloc(state->samplerate * 2 / 5
                                     * state->channels
                                     * sizeof(double));
  CHECK_ERROR(!state->audio_data, "Could not allocate memory!\n", 0, free_channel_map)
  errcode = ebur128_init_multi_array(&(state->v), state->channels, 5);
  CHECK_ERROR(errcode, "Could not allocate memory!\n", 0, free_audio_data)
  errcode = ebur128_init_filter(state);
  CHECK_ERROR(errcode, "Could not initialize filter!\n", 0, free_v)

  LIST_INIT(&state->block_list);
  state->block_counter = 0;

  /* the first block needs 400ms of audio data */
  state->needed_frames = state->samplerate * 2 / 5;
  /* start at the beginning of the buffer */
  state->audio_data_index = 0;

  return state;

free_v:
  ebur128_release_multi_array(&(state->v), state->channels);
free_audio_data:
  free(state->audio_data);
free_channel_map:
  free(state->channel_map);
free_state:
  free(state);
exit:
  return NULL;
}

int ebur128_destroy(ebur128_state** st) {
  struct ebur128_dq_entry* entry;
  free((*st)->audio_data);
  free((*st)->channel_map);
  ebur128_release_multi_array(&(*st)->v, (*st)->channels);
  free((*st)->a);
  free((*st)->b);
  while ((*st)->block_list.lh_first != NULL) {
    entry = (*st)->block_list.lh_first;
    LIST_REMOVE((*st)->block_list.lh_first, entries);
    free(entry);
  }

  free(*st);
  *st = NULL;

  return 0;
}

int ebur128_filter(ebur128_state* st, size_t frames) {
  double* audio_data = st->audio_data + st->audio_data_index;
  size_t i, c;
  for (c = 0; c < st->channels; ++c) {
    if (st->channel_map[c] == EBUR128_UNUSED) continue;
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

int ebur128_calc_gating_block(ebur128_state* st) {
  size_t i, c;
  struct ebur128_dq_entry* block;
  block = malloc(sizeof(struct ebur128_dq_entry));
  if (!block) return 1;
  block->z = 0.0;
  for (c = 0; c < st->channels; ++c) {
    double sum = 0.0;
    if (st->channel_map[c] == EBUR128_UNUSED) continue;
    for (i = 0; i < st->samplerate * 2 / 5; ++i) {
      sum += st->audio_data[i * st->channels + c] *
             st->audio_data[i * st->channels + c];
    }
    if (st->channel_map[c] == EBUR128_LEFT_SURROUND ||
        st->channel_map[c] == EBUR128_RIGHT_SURROUND) {
      sum *= 1.41;
    }
    block->z += sum;
  }
  LIST_INSERT_HEAD(&st->block_list, block, entries);
  return 0;
}

void ebur128_set_channel_map(ebur128_state* st,
                            int* channel_map) {
  memcpy(st->channel_map, channel_map, st->channels * sizeof(int));
}

int ebur128_write_frames(ebur128_state* st, const double* src, size_t frames) {
  int errcode = 0;
  size_t src_index = 0;
  while (frames > 0) {
    if (frames >= st->needed_frames) {
      memcpy(&st->audio_data[st->audio_data_index], &src[src_index],
             st->needed_frames * st->channels * sizeof(double));
      src_index += st->needed_frames * st->channels;
      frames -= st->needed_frames;
      ebur128_filter(st, st->needed_frames);
      errcode = ebur128_calc_gating_block(st);
      if (errcode) return 1;
      ++st->block_counter;
      st->audio_data_index += st->needed_frames * st->channels;
      if (st->audio_data_index == st->samplerate * 2 / 5 * st->channels) {
        st->audio_data_index = 0;
      }
      st->needed_frames = st->samplerate / 5;
    } else {
      memcpy(&st->audio_data[st->audio_data_index], &src[src_index],
             frames * st->channels * sizeof(double));
      ebur128_filter(st, frames);
      st->audio_data_index += frames * st->channels;
      st->needed_frames -= frames;
      frames = 0;
    }
  }
  return 0;
}

void ebur128_start_new_segment(ebur128_state* st) {
  st->block_counter = 0;
}

double ebur128_relative_threshold(ebur128_state* st, size_t block_count) {
  struct ebur128_dq_entry* it;
  double relative_threshold = 0.0;
  int above_thresh_counter = 0;
  double threshold = 1.1724653045822964e-7 * (double) (st->samplerate * 2 / 5);
  for (it = st->block_list.lh_first; it != NULL;
       it = it->entries.le_next) {
    if (it->z >= threshold) {
      ++above_thresh_counter;
      relative_threshold += it->z;
    }
    --block_count;
    if (!block_count) break;
  }
  relative_threshold /= above_thresh_counter;
  return 0.1584893192461113 * relative_threshold;
}

double ebur128_gated_loudness(ebur128_state* st, size_t block_count) {
  double relative_threshold = ebur128_relative_threshold(st, block_count);
  struct ebur128_dq_entry* it;
  double gated_loudness = 0.0;
  int above_thresh_counter = 0;
  for (it = st->block_list.lh_first; it != NULL;
       it = it->entries.le_next) {
    if (it->z >= relative_threshold) {
      ++above_thresh_counter;
      gated_loudness += it->z;
    }
    --block_count;
    if (!block_count) break;
  }
  gated_loudness /= above_thresh_counter * (double) (st->samplerate * 2 / 5);
  gated_loudness = 10 * (log(gated_loudness) / log(10.0));
  gated_loudness -= 0.691;
  return gated_loudness;
}

double ebur128_gated_loudness_global(ebur128_state* st) {
  return ebur128_gated_loudness(st, 0);
}

double ebur128_gated_loudness_segment(ebur128_state* st) {
  return ebur128_gated_loudness(st, st->block_counter);
}

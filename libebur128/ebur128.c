/* See LICENSE file for copyright and license details. */
#include "ebur128.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#if EBUR128_USE_SPEEX_RESAMPLER
  #define OUTSIDE_SPEEX
  #define RANDOM_PREFIX ebur128
  #include "speex_resampler.h"
#endif

/* This can be replaced by any BSD-like queue implementation. */
#include "queue.h"

#define PI 3.14159265358979323846

#define CHECK_ERROR(condition, message, errorcode, goto_point)                 \
  if ((condition)) {                                                           \
    fprintf(stderr, message);                                                  \
    errcode = (errorcode);                                                     \
    goto goto_point;                                                           \
  }

SLIST_HEAD(ebur128_double_queue, ebur128_dq_entry);
struct ebur128_dq_entry {
  double z;
  SLIST_ENTRY(ebur128_dq_entry) entries;
};

struct ebur128_state_internal {
  /** Filtered audio data (used as ring buffer). */
  double* audio_data;
  /** Size of audio_data array. */
  size_t audio_data_frames;
  /** Current index for audio_data. */
  size_t audio_data_index;
  /** How many frames are needed for a gating block. Will correspond to 400ms
   *  of audio at initialization, and 100ms after the first block (75% overlap
   *  as specified in the 2011 revision of BS1770). */
  size_t needed_frames;
  /** The channel map. Has as many elements as there are channels. */
  int* channel_map;
  /** How many samples fit in 100ms (rounded). */
  size_t samples_in_100ms;
  /** BS.1770 filter coefficients (nominator). */
  double b[5];
  /** BS.1770 filter coefficients (denominator). */
  double a[5];
  /** BS.1770 filter state. */
  double v[5][5];
  /** Linked list of block energies. */
  struct ebur128_double_queue block_list;
  /** Linked list of 3s-block energies, used to calculate LRA. */
  struct ebur128_double_queue short_term_block_list;
  /** Keeps track of when a new short term block is needed. */
  size_t short_term_frame_counter;
  /** Maximum sample peak, one per channel */
  double* sample_peak;
  /** Maximum true peak, one per channel */
  double* true_peak;
#if EBUR128_USE_SPEEX_RESAMPLER
  SpeexResamplerState* resampler;
#endif
  size_t oversample_factor;
  float* resampler_buffer_input;
  size_t resampler_buffer_input_frames;
  float* resampler_buffer_output;
  size_t resampler_buffer_output_frames;
};

double relative_gate = -8.0;

/* Those will be calculated when initializing the library */
static double relative_gate_factor;
static double minus_twenty_decibels;
static double abs_threshold_energy;

int ebur128_init_filter(ebur128_state* st) {
  double f0 = 1681.974450955533;
  double G  =    3.999843853973347;
  double Q  =    0.7071752369554196;

  double K  = tan(PI * f0 / (double) st->samplerate);
  double Vh = pow(10.0, G / 20.0);
  double Vb = pow(Vh, 0.4996667741545416);

  double pb[3] = {0.0,  0.0, 0.0};
  double pa[3] = {1.0,  0.0, 0.0};
  double rb[3] = {1.0, -2.0, 1.0};
  double ra[3] = {1.0,  0.0, 0.0};

  double a0 =      1.0 + K / Q + K * K      ;
  pb[0] =     (Vh + Vb * K / Q + K * K) / a0;
  pb[1] =           2.0 * (K * K -  Vh) / a0;
  pb[2] =     (Vh - Vb * K / Q + K * K) / a0;
  pa[1] =           2.0 * (K * K - 1.0) / a0;
  pa[2] =         (1.0 - K / Q + K * K) / a0;

  /* fprintf(stderr, "%.14f %.14f %.14f %.14f %.14f\n",
                     b1[0], b1[1], b1[2], a1[1], a1[2]); */

  f0 = 38.13547087602444;
  Q  =  0.5003270373238773;
  K  = tan(PI * f0 / (double) st->samplerate);

  ra[1] =   2.0 * (K * K - 1.0) / (1.0 + K / Q + K * K);
  ra[2] = (1.0 - K / Q + K * K) / (1.0 + K / Q + K * K);

  /* fprintf(stderr, "%.14f %.14f\n", a2[1], a2[2]); */

  st->d->b[0] = pb[0] * rb[0];
  st->d->b[1] = pb[0] * rb[1] + pb[1] * rb[0];
  st->d->b[2] = pb[0] * rb[2] + pb[1] * rb[1] + pb[2] * rb[0];
  st->d->b[3] = pb[1] * rb[2] + pb[2] * rb[1];
  st->d->b[4] = pb[2] * rb[2];

  st->d->a[0] = pa[0] * ra[0];
  st->d->a[1] = pa[0] * ra[1] + pa[1] * ra[0];
  st->d->a[2] = pa[0] * ra[2] + pa[1] * ra[1] + pa[2] * ra[0];
  st->d->a[3] = pa[1] * ra[2] + pa[2] * ra[1];
  st->d->a[4] = pa[2] * ra[2];

  memset(st->d->v, '\0', 25 * sizeof(double));

  return 0;
}

int ebur128_init_channel_map(ebur128_state* st) {
  size_t i;
  st->d->channel_map = (int*) calloc(st->channels, sizeof(int));
  if (!st->d->channel_map) return 1;
  for (i = 0; i < st->channels; ++i) {
    switch (i) {
      case 0:  st->d->channel_map[i] = EBUR128_LEFT;           break;
      case 1:  st->d->channel_map[i] = EBUR128_RIGHT;          break;
      case 2:  st->d->channel_map[i] = EBUR128_CENTER;         break;
      case 3:  st->d->channel_map[i] = EBUR128_UNUSED;         break;
      case 4:  st->d->channel_map[i] = EBUR128_LEFT_SURROUND;  break;
      case 5:  st->d->channel_map[i] = EBUR128_RIGHT_SURROUND; break;
      default: st->d->channel_map[i] = EBUR128_UNUSED;         break;
    }
  }
  return 0;
}

#if EBUR128_USE_SPEEX_RESAMPLER
int ebur128_init_resampler(ebur128_state* st) {
  int errcode = 0, result;

  if (st->samplerate < 96000) {
    st->d->oversample_factor = 4;
  } else if (st->samplerate < 192000) {
    st->d->oversample_factor = 2;
  } else {
    st->d->oversample_factor = 1;
    st->d->resampler_buffer_input = NULL;
    st->d->resampler_buffer_output = NULL;
    st->d->resampler = NULL;
  }

  st->d->resampler_buffer_input_frames = st->d->samples_in_100ms * 4;
  st->d->resampler_buffer_input = calloc(st->d->resampler_buffer_input_frames *
                                      st->channels *
                                      sizeof(float), 1);
  CHECK_ERROR(!st->d->resampler_buffer_input, "Could not allocate memory!\n", 1,
                                           exit)

  st->d->resampler_buffer_output_frames =
                                    st->d->resampler_buffer_input_frames *
                                    st->d->oversample_factor;
  st->d->resampler_buffer_output = calloc
                                      (st->d->resampler_buffer_output_frames *
                                       st->channels *
                                       sizeof(float), 1);
  CHECK_ERROR(!st->d->resampler_buffer_output, "Could not allocate memory!\n",
                                            1, free_input)

  st->d->resampler = ebur128_resampler_init
                 ((spx_uint32_t) st->channels,
                  (spx_uint32_t) st->samplerate,
                  (spx_uint32_t) (st->samplerate * st->d->oversample_factor),
                  8,
                  &result);
  CHECK_ERROR(!st->d->resampler, "Could not initialize resampler!\n", 1,
                              free_output)

  return 0;

free_input:
  free(st->d->resampler_buffer_input);
  st->d->resampler_buffer_input = NULL;
free_output:
  free(st->d->resampler_buffer_output);
  st->d->resampler_buffer_output = NULL;
exit:
  return errcode;
}

void ebur128_destroy_resampler(ebur128_state* st) {
  free(st->d->resampler_buffer_input);
  st->d->resampler_buffer_input = NULL;
  free(st->d->resampler_buffer_output);
  st->d->resampler_buffer_output = NULL;
  ebur128_resampler_destroy(st->d->resampler);
  st->d->resampler = NULL;
}
#endif

ebur128_state* ebur128_init(size_t channels, size_t samplerate, int mode) {
  int errcode;
  ebur128_state* st;

  st = (ebur128_state*) malloc(sizeof(ebur128_state));
  CHECK_ERROR(!st, "Could not allocate memory!\n", 0, exit)
  st->d = (struct ebur128_state_internal*)
          malloc(sizeof(struct ebur128_state_internal));
  CHECK_ERROR(!st->d, "Could not allocate memory!\n", 0, free_state)
  st->channels = channels;
  errcode = ebur128_init_channel_map(st);
  CHECK_ERROR(errcode, "Could not initialize channel map!\n", 0, free_internal)

  st->d->sample_peak = (double*) calloc(channels, sizeof(double));
  CHECK_ERROR(!st->d->sample_peak, "Could not allocate memory!\n", 0,
                                  free_channel_map)
  st->d->true_peak = (double*) calloc(channels, sizeof(double));
  CHECK_ERROR(!st->d->true_peak, "Could not allocate memory!\n", 0,
                                  free_sample_peak)

  st->samplerate = samplerate;
  st->d->samples_in_100ms = (st->samplerate + 5) / 10;
  st->mode = mode;
  if ((mode & EBUR128_MODE_S) == EBUR128_MODE_S) {
    st->d->audio_data_frames = st->d->samples_in_100ms * 30;
  } else if ((mode & EBUR128_MODE_M) == EBUR128_MODE_M) {
    st->d->audio_data_frames = st->d->samples_in_100ms * 4;
  } else {
    return NULL;
  }
  st->d->audio_data = (double*) calloc(st->d->audio_data_frames *
                                   st->channels,
                                   sizeof(double));
  CHECK_ERROR(!st->d->audio_data, "Could not allocate memory!\n", 0,
                               free_true_peak)
  ebur128_init_filter(st);

  SLIST_INIT(&st->d->block_list);
  SLIST_INIT(&st->d->short_term_block_list);
  st->d->short_term_frame_counter = 0;

#if EBUR128_USE_SPEEX_RESAMPLER
  ebur128_init_resampler(st);
#endif

  /* the first block needs 400ms of audio data */
  st->d->needed_frames = st->d->samples_in_100ms * 4;
  /* start at the beginning of the buffer */
  st->d->audio_data_index = 0;

  /* initialize static constants */
  relative_gate_factor = pow(10.0, relative_gate / 10.0);
  minus_twenty_decibels = pow(10.0, -20.0 / 10.0);
  abs_threshold_energy = pow(10.0, (-70.0 + 0.691) / 10.0);

  return st;

free_true_peak:
  free(st->d->true_peak);
free_sample_peak:
  free(st->d->sample_peak);
free_channel_map:
  free(st->d->channel_map);
free_internal:
  free(st->d);
free_state:
  free(st);
exit:
  return NULL;
}

void ebur128_destroy(ebur128_state** st) {
  struct ebur128_dq_entry* entry;
  free((*st)->d->audio_data);
  free((*st)->d->channel_map);
  free((*st)->d->sample_peak);
  free((*st)->d->true_peak);
  while (!SLIST_EMPTY(&(*st)->d->block_list)) {
    entry = SLIST_FIRST(&(*st)->d->block_list);
    SLIST_REMOVE_HEAD(&(*st)->d->block_list, entries);
    free(entry);
  }
  while (!SLIST_EMPTY(&(*st)->d->short_term_block_list)) {
    entry = SLIST_FIRST(&(*st)->d->short_term_block_list);
    SLIST_REMOVE_HEAD(&(*st)->d->short_term_block_list, entries);
    free(entry);
  }
#if EBUR128_USE_SPEEX_RESAMPLER
  ebur128_destroy_resampler(*st);
#endif

  free((*st)->d);
  free(*st);
  *st = NULL;
}

int ebur128_use_speex_resampler(ebur128_state* st) {
#if EBUR128_USE_SPEEX_RESAMPLER
  return ((st->mode & EBUR128_MODE_TRUE_PEAK) == EBUR128_MODE_TRUE_PEAK);
#else
  (void) st;
  return 0;
#endif
}

void ebur128_check_true_peak(ebur128_state* st, size_t frames) {
#if EBUR128_USE_SPEEX_RESAMPLER
  size_t c, i;
  spx_uint32_t in_len = (spx_uint32_t) frames;
  spx_uint32_t out_len = (spx_uint32_t) st->d->resampler_buffer_output_frames;
  ebur128_resampler_process_interleaved_float(
                      st->d->resampler,
                      st->d->resampler_buffer_input,  &in_len,
                      st->d->resampler_buffer_output, &out_len);
  for (c = 0; c < st->channels; ++c) {
    for (i = 0; i < out_len; ++i) {
      if (st->d->resampler_buffer_output[i * st->channels + c] >
                                                         st->d->true_peak[c]) {
        st->d->true_peak[c] =
            st->d->resampler_buffer_output[i * st->channels + c];
      } else if (-st->d->resampler_buffer_output[i * st->channels + c] >
                                                         st->d->true_peak[c]) {
        st->d->true_peak[c] =
           -st->d->resampler_buffer_output[i * st->channels + c];
      }
    }
  }
#else
  (void) st; (void) frames;
#endif
}

#define EBUR128_FILTER(type, min_scale, max_scale)                             \
int ebur128_filter_##type(ebur128_state* st, const type* src, size_t frames) { \
  static double scaling_factor = -((double) min_scale) > (double) max_scale ?  \
                                 -((double) min_scale) : (double) max_scale;   \
  double* audio_data = st->d->audio_data + st->d->audio_data_index;            \
  size_t i, c;                                                                 \
  if ((st->mode & EBUR128_MODE_SAMPLE_PEAK) == EBUR128_MODE_SAMPLE_PEAK) {     \
    for (c = 0; c < st->channels; ++c) {                                       \
      double max = 0.0;                                                        \
      for (i = 0; i < frames; ++i) {                                           \
        if (src[i * st->channels + c] > max) {                                 \
          max =  src[i * st->channels + c];                                    \
        } else if (-src[i * st->channels + c] > max) {                         \
          max = -src[i * st->channels + c];                                    \
        }                                                                      \
      }                                                                        \
      max /= scaling_factor;                                                   \
      if (max > st->d->sample_peak[c]) st->d->sample_peak[c] = max;            \
    }                                                                          \
  }                                                                            \
  if (ebur128_use_speex_resampler(st)) {                                       \
    for (c = 0; c < st->channels; ++c) {                                       \
      for (i = 0; i < frames; ++i) {                                           \
        st->d->resampler_buffer_input[i * st->channels + c] =                  \
                      (float) (src[i * st->channels + c] / scaling_factor);    \
      }                                                                        \
    }                                                                          \
    ebur128_check_true_peak(st, frames);                                       \
  }                                                                            \
  for (c = 0; c < st->channels; ++c) {                                         \
    int ci = st->d->channel_map[c] - 1;                                        \
    if (ci < 0) continue;                                                      \
    for (i = 0; i < frames; ++i) {                                             \
      st->d->v[ci][0] = (double) (src[i * st->channels + c] / scaling_factor)  \
                   - st->d->a[1] * st->d->v[ci][1]                             \
                   - st->d->a[2] * st->d->v[ci][2]                             \
                   - st->d->a[3] * st->d->v[ci][3]                             \
                   - st->d->a[4] * st->d->v[ci][4];                            \
      audio_data[i * st->channels + c] =                                       \
                     st->d->b[0] * st->d->v[ci][0]                             \
                   + st->d->b[1] * st->d->v[ci][1]                             \
                   + st->d->b[2] * st->d->v[ci][2]                             \
                   + st->d->b[3] * st->d->v[ci][3]                             \
                   + st->d->b[4] * st->d->v[ci][4];                            \
      st->d->v[ci][4] = st->d->v[ci][3];                                       \
      st->d->v[ci][3] = st->d->v[ci][2];                                       \
      st->d->v[ci][2] = st->d->v[ci][1];                                       \
      st->d->v[ci][1] = st->d->v[ci][0];                                       \
    }                                                                          \
    /* prevent denormal numbers */                                             \
    st->d->v[ci][4] = fabs(st->d->v[ci][4]) < 1.0e-15 ? 0.0 : st->d->v[ci][4]; \
    st->d->v[ci][3] = fabs(st->d->v[ci][3]) < 1.0e-15 ? 0.0 : st->d->v[ci][3]; \
    st->d->v[ci][2] = fabs(st->d->v[ci][2]) < 1.0e-15 ? 0.0 : st->d->v[ci][2]; \
    st->d->v[ci][1] = fabs(st->d->v[ci][1]) < 1.0e-15 ? 0.0 : st->d->v[ci][1]; \
  }                                                                            \
  return 0;                                                                    \
}
EBUR128_FILTER(short, SHRT_MIN, SHRT_MAX)
EBUR128_FILTER(int, INT_MIN, INT_MAX)
EBUR128_FILTER(float, -1.0f, 1.0f)
EBUR128_FILTER(double, -1.0, 1.0)

int ebur128_calc_gating_block(ebur128_state* st, size_t frames_per_block,
                              double* optional_output) {
  size_t i, c;
  double sum = 0.0;
  double channel_sum;
  for (c = 0; c < st->channels; ++c) {
    if (st->d->channel_map[c] == EBUR128_UNUSED) continue;
    channel_sum = 0.0;
    if (st->d->audio_data_index < frames_per_block * st->channels) {
      for (i = 0; i < st->d->audio_data_index / st->channels; ++i) {
        channel_sum += st->d->audio_data[i * st->channels + c] *
                       st->d->audio_data[i * st->channels + c];
      }
      for (i = st->d->audio_data_frames -
              (frames_per_block -
               st->d->audio_data_index / st->channels);
           i < st->d->audio_data_frames; ++i) {
        channel_sum += st->d->audio_data[i * st->channels + c] *
                       st->d->audio_data[i * st->channels + c];
      }
    } else {
      for (i = st->d->audio_data_index / st->channels - frames_per_block;
           i < st->d->audio_data_index / st->channels;
           ++i) {
        channel_sum += st->d->audio_data[i * st->channels + c] *
                       st->d->audio_data[i * st->channels + c];
      }
    }
    if (st->d->channel_map[c] == EBUR128_LEFT_SURROUND ||
        st->d->channel_map[c] == EBUR128_RIGHT_SURROUND) {
      channel_sum *= 1.41;
    }
    sum += channel_sum;
  }
  sum /= (double) frames_per_block;
  if (optional_output) {
    *optional_output = sum;
    return 0;
  } else if (sum >= abs_threshold_energy) {
    struct ebur128_dq_entry* block;
    block = (struct ebur128_dq_entry*) malloc(sizeof(struct ebur128_dq_entry));
    if (!block) return -1;
    block->z = sum;
    SLIST_INSERT_HEAD(&st->d->block_list, block, entries);
    return 0;
  } else {
    return 1;
  }
}

int ebur128_set_channel(ebur128_state* st, size_t channel_number, int value) {
  if (channel_number >= st->channels) {
    return 1;
  }
  st->d->channel_map[channel_number] = value;
  return 0;
}

int ebur128_change_parameters(ebur128_state* st,
                              size_t channels,
                              size_t samplerate) {
  int errcode;
  if (channels == st->channels &&
      samplerate == st->samplerate) {
    return 2;
  }
  free(st->d->audio_data);
  st->d->audio_data = NULL;

  if (channels != st->channels) {
    free(st->d->channel_map); st->d->channel_map = NULL;
    free(st->d->sample_peak); st->d->sample_peak = NULL;
    free(st->d->true_peak);   st->d->true_peak = NULL;
    st->channels = channels;

#if EBUR128_USE_SPEEX_RESAMPLER
    ebur128_destroy_resampler(st);
    ebur128_init_resampler(st);
#endif

    errcode = ebur128_init_channel_map(st);
    CHECK_ERROR(errcode, "Could not initialize channel map!\n", 1, exit)

    st->d->sample_peak = (double*) calloc(channels, sizeof(double));
    CHECK_ERROR(!st->d->sample_peak, "Could not allocate memory!\n", 1, exit)
    st->d->true_peak = (double*) calloc(channels, sizeof(double));
    CHECK_ERROR(!st->d->true_peak, "Could not allocate memory!\n", 1, exit)
  }
  if (samplerate != st->samplerate) {
    st->samplerate = samplerate;
    ebur128_init_filter(st);
  }
  if ((st->mode & EBUR128_MODE_S) == EBUR128_MODE_S) {
    st->d->audio_data_frames = st->d->samples_in_100ms * 30;
  } else if ((st->mode & EBUR128_MODE_M) == EBUR128_MODE_M) {
    st->d->audio_data_frames = st->d->samples_in_100ms * 4;
  } else {
    return 1;
  }
  st->d->audio_data = (double*) calloc(st->d->audio_data_frames *
                                   st->channels,
                                   sizeof(double));
  CHECK_ERROR(!st->d->audio_data, "Could not allocate memory!\n", 1, exit)

  /* the first block needs 400ms of audio data */
  st->d->needed_frames = st->d->samples_in_100ms * 4;
  /* start at the beginning of the buffer */
  st->d->audio_data_index = 0;
  /* reset short term frame counter */
  st->d->short_term_frame_counter = 0;

  return 0;

exit:
  return 1;
}


double ebur128_energy_shortterm(ebur128_state* st);
#define EBUR128_ADD_FRAMES(type)                                               \
int ebur128_add_frames_##type(ebur128_state* st,                               \
                              const type* src, size_t frames) {                \
  int errcode = 0;                                                             \
  size_t src_index = 0;                                                        \
  while (frames > 0) {                                                         \
    if (frames >= st->d->needed_frames) {                                      \
      ebur128_filter_##type(st, src + src_index, st->d->needed_frames);        \
      src_index += st->d->needed_frames * st->channels;                        \
      frames -= st->d->needed_frames;                                          \
      st->d->audio_data_index += st->d->needed_frames * st->channels;          \
      /* calculate the new gating block */                                     \
      if ((st->mode & EBUR128_MODE_I) == EBUR128_MODE_I) {                     \
        errcode = ebur128_calc_gating_block(st,                                \
                                            st->d->samples_in_100ms * 4, NULL);\
        if (errcode == -1) return 1;                                           \
      }                                                                        \
      if ((st->mode & EBUR128_MODE_LRA) == EBUR128_MODE_LRA) {                 \
        st->d->short_term_frame_counter += st->d->needed_frames;               \
        if (st->d->short_term_frame_counter == st->d->samples_in_100ms * 30) { \
          double st_energy = ebur128_energy_shortterm(st);                     \
          struct ebur128_dq_entry* block;                                      \
          block = (struct ebur128_dq_entry*)                                   \
                  malloc(sizeof(struct ebur128_dq_entry));                     \
          if (!block) return 1;                                                \
          block->z = st_energy;                                                \
          SLIST_INSERT_HEAD(&st->d->short_term_block_list, block, entries);    \
          st->d->short_term_frame_counter = st->d->samples_in_100ms * 20;      \
        }                                                                      \
      }                                                                        \
      /* 100ms are needed for all blocks besides the first one */              \
      st->d->needed_frames = st->d->samples_in_100ms;                          \
      /* reset audio_data_index when buffer full */                            \
      if (st->d->audio_data_index == st->d->audio_data_frames * st->channels) {\
        st->d->audio_data_index = 0;                                           \
      }                                                                        \
    } else {                                                                   \
      ebur128_filter_##type(st, src + src_index, frames);                      \
      st->d->audio_data_index += frames * st->channels;                        \
      if ((st->mode & EBUR128_MODE_LRA) == EBUR128_MODE_LRA) {                 \
        st->d->short_term_frame_counter += frames;                             \
      }                                                                        \
      st->d->needed_frames -= frames;                                          \
      frames = 0;                                                              \
    }                                                                          \
  }                                                                            \
  return 0;                                                                    \
}
EBUR128_ADD_FRAMES(short)
EBUR128_ADD_FRAMES(int)
EBUR128_ADD_FRAMES(float)
EBUR128_ADD_FRAMES(double)

double ebur128_energy_to_loudness(double energy) {
  return 10 * (log(energy) / log(10.0)) - 0.691;
}

double ebur128_gated_loudness(ebur128_state** sts, size_t size,
                              size_t block_count) {
  struct ebur128_dq_entry* it;
  double relative_threshold = 0.0;
  double gated_loudness = 0.0;
  size_t above_thresh_counter = 0;
  size_t i;

  for (i = 0; i < size; i++) {
    if (sts[i] && (sts[i]->mode & EBUR128_MODE_I) != EBUR128_MODE_I) {
      return 0.0 / 0.0;
    }
  }

  for (i = 0; i < size; i++) {
    if (!sts[i]) continue;
    SLIST_FOREACH(it, &sts[i]->d->block_list, entries) {
      if (above_thresh_counter >= block_count) break;
      ++above_thresh_counter;
      relative_threshold += it->z;
    }
  }
  if (!above_thresh_counter) return -1.0 / 0.0;
  relative_threshold /= (double) above_thresh_counter;
  relative_threshold *= relative_gate_factor;
  above_thresh_counter = 0;
  for (i = 0; i < size; i++) {
    if (!sts[i]) continue;
    SLIST_FOREACH(it, &sts[i]->d->block_list, entries) {
      if (block_count == 0) break;
      if (it->z >= relative_threshold) {
        ++above_thresh_counter;
        gated_loudness += it->z;
      }
      --block_count;
    }
  }
  if (!above_thresh_counter) return -1.0 / 0.0;
  gated_loudness /= (double) above_thresh_counter;
  return ebur128_energy_to_loudness(gated_loudness);
}

double ebur128_loudness_global(ebur128_state* st) {
  return ebur128_gated_loudness(&st, 1, (size_t) -1);
}

double ebur128_loudness_global_multiple(ebur128_state** sts, size_t size) {
  return ebur128_gated_loudness(sts, size, (size_t) -1);
}

double ebur128_energy_in_interval(ebur128_state* st, size_t interval_frames) {
  double loudness;

  if (interval_frames > st->d->audio_data_frames) return 0.0 / 0.0;
  ebur128_calc_gating_block(st, interval_frames, &loudness);
  return loudness;
}

double ebur128_loudness_momentary(ebur128_state* st) {
  double energy = ebur128_energy_in_interval(st, st->d->samples_in_100ms * 4);
  return ebur128_energy_to_loudness(energy);
}

double ebur128_energy_shortterm(ebur128_state* st) {
  return ebur128_energy_in_interval(st, st->d->samples_in_100ms * 30);
}

double ebur128_loudness_shortterm(ebur128_state* st) {
  double energy = ebur128_energy_shortterm(st);
  return ebur128_energy_to_loudness(energy);
}

static int ebur128_double_cmp(const void *p1, const void *p2) {
  const double* d1 = (const double*) p1;
  const double* d2 = (const double*) p2;
  return (*d1 > *d2) - (*d1 < *d2);
}

/* EBU - TECH 3342 */
double ebur128_loudness_range_multiple(ebur128_state** sts, size_t size) {
  size_t i, j;
  struct ebur128_dq_entry* it;
  double* stl_vector;
  size_t stl_size = 0;
  double* stl_abs_gated;
  size_t stl_abs_gated_size;
  double* stl_relgated;
  size_t stl_relgated_size;
  double stl_power = 0.0, stl_integrated;
  /* High and low percentile energy */
  double h_en, l_en;

  for (i = 0; i < size; ++i) {
    if (sts[i] && (sts[i]->mode & EBUR128_MODE_LRA) != EBUR128_MODE_LRA) {
      return 0.0 / 0.0;
    }
  }

  for (i = 0; i < size; ++i) {
    if (!sts[i]) continue;
    SLIST_FOREACH(it, &sts[i]->d->short_term_block_list, entries) {
      ++stl_size;
    }
  }
  if (!stl_size) return 0.0;
  stl_vector = (double*) calloc(stl_size, sizeof(double));
  if (!stl_vector) return 0.0 / 0.0;
  j = 0;
  for (i = 0; i < size; ++i) {
    if (!sts[i]) continue;
    SLIST_FOREACH(it, &sts[i]->d->short_term_block_list, entries) {
      stl_vector[j] = it->z;
      ++j;
    }
  }
  qsort(stl_vector, stl_size, sizeof(double), ebur128_double_cmp);
  stl_abs_gated = stl_vector;
  stl_abs_gated_size = stl_size;
  while (stl_abs_gated_size > 0 && *stl_abs_gated < abs_threshold_energy) {
    ++stl_abs_gated;
    --stl_abs_gated_size;
  }
  for (i = 0; i < stl_abs_gated_size; ++i) {
    stl_power += stl_abs_gated[i];
  }
  stl_power /= (double) stl_abs_gated_size;
  stl_integrated = minus_twenty_decibels * stl_power;

  stl_relgated = stl_abs_gated;
  stl_relgated_size = stl_abs_gated_size;
  while (stl_relgated_size > 0 && *stl_relgated < stl_integrated) {
    ++stl_relgated;
    --stl_relgated_size;
  }

  if (stl_relgated_size) {
    h_en = stl_relgated[(size_t) ((double) (stl_relgated_size - 1) * 0.95 +
                                  0.5)];
    l_en = stl_relgated[(size_t) ((double) (stl_relgated_size - 1) * 0.1 +
                                  0.5)];
    free(stl_vector);
    return ebur128_energy_to_loudness(h_en) - ebur128_energy_to_loudness(l_en);
  } else {
    free(stl_vector);
    return 0.0;
  }
}

double ebur128_loudness_range(ebur128_state* st) {
  return ebur128_loudness_range_multiple(&st, 1);
}

double ebur128_sample_peak(ebur128_state* st, size_t channel_number) {
  if ((st->mode & EBUR128_MODE_SAMPLE_PEAK) != EBUR128_MODE_SAMPLE_PEAK ||
       channel_number >= st->channels) {
    return 0.0 / 0.0;
  }
  return st->d->sample_peak[channel_number];
}

#if EBUR128_USE_SPEEX_RESAMPLER
double ebur128_true_peak(ebur128_state* st, size_t channel_number) {
  if ((st->mode & EBUR128_MODE_TRUE_PEAK) != EBUR128_MODE_TRUE_PEAK ||
       channel_number >= st->channels) {
    return 0.0 / 0.0;
  }
  return st->d->true_peak[channel_number];
}
#endif

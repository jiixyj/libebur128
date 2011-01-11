/* See LICENSE file for copyright and license details. */
#ifndef _EBUR128_H_
#define _EBUR128_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>

/* This can be replaced by any BSD-like queue implementation */
#include "queue.h"

LIST_HEAD(ebur128_double_queue, ebur128_dq_entry);
struct ebur128_dq_entry {
  double z;
  LIST_ENTRY(ebur128_dq_entry) entries;
};

/* Use these values when setting the channel map */
enum channels {
  EBUR128_UNUSED = 0,
  EBUR128_LEFT,
  EBUR128_RIGHT,
  EBUR128_CENTER,
  EBUR128_LEFT_SURROUND,
  EBUR128_RIGHT_SURROUND
};

/* Use these values in ebur128_init */
enum mode {
  EBUR128_MODE_M_I,
  EBUR128_MODE_M_S_I,
  EBUR128_MODE_M,
  EBUR128_MODE_M_S
};

typedef struct {
  size_t mode;
  double* audio_data;
  size_t audio_data_frames;
  size_t audio_data_index;
  size_t needed_frames;
  size_t channels;
  int* channel_map;
  size_t samplerate;
  double* a;
  double* b;
  double** v;
  struct ebur128_double_queue block_list;
  size_t block_counter;
} ebur128_state;

ebur128_state* ebur128_init(int channels, int samplerate, size_t mode);
int ebur128_destroy(ebur128_state** st);

/* The length of channel_map should be equal to the number of channels.
 * The default is: {EBUR128_LEFT,
 *                  EBUR128_RIGHT,
 *                  EBUR128_CENTER,
 *                  EBUR128_UNUSED,
 *                  EBUR128_LEFT_SURROUND,
 *                  EBUR128_RIGHT_SURROUND} */
void ebur128_set_channel_map(ebur128_state* st, int* channel_map);

int ebur128_write_frames(ebur128_state* st, const double* src, size_t frames);
void ebur128_start_new_segment(ebur128_state* st);

/* Get integrated loudness of the last segment/track. After this you should
 * start a new segment with ebur128_start_new_segment */
double ebur128_gated_loudness_segment(ebur128_state* st);
/* Get integrated loudness of the whole programme */
double ebur128_gated_loudness_global(ebur128_state* st);
/* Get momentary loudness (last 400ms) */
double ebur128_loudness_momentary(ebur128_state* st);
/* Get short-term loudness (last 3s) */
double ebur128_loudness_shortterm(ebur128_state* st);

#ifdef __cplusplus
}
#endif

#endif  /* _EBUR128_H_ */

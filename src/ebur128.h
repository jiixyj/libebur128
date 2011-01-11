#ifndef _EBUR128_H_
#define _EBUR128_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include <sys/queue.h>

LIST_HEAD(ebur128_double_queue, ebur128_dq_entry);
struct ebur128_dq_entry {
  double z;
  LIST_ENTRY(ebur128_dq_entry) entries;
};

enum channels {
  EBUR128_UNUSED = 0,
  EBUR128_LEFT,
  EBUR128_RIGHT,
  EBUR128_CENTER,
  EBUR128_LEFT_SURROUND,
  EBUR128_RIGHT_SURROUND
};

enum mode {
  EBUR128_MODE_M_I,
  EBUR128_MODE_M_S_I
};

typedef struct {
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

ebur128_state* ebur128_init(int channels, int samplerate, int mode);
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

double ebur128_gated_loudness_segment(ebur128_state* st);
double ebur128_gated_loudness_global(ebur128_state* st);
double ebur128_loudness_momentary(ebur128_state* st);
double ebur128_loudness_shortterm(ebur128_state* st);

#ifdef __cplusplus
}
#endif

#endif  /* _EBUR128_H_ */

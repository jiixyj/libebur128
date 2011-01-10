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

typedef struct {
  double* audio_data;
  size_t audio_data_index;
  size_t channels;
  size_t samplerate;
  double* a;
  double* b;
  double** v;
  struct ebur128_double_queue block_list;
  size_t block_counter;
} ebur128_state;

ebur128_state* ebur128_init(int channels, int samplerate);
int ebur128_destroy(ebur128_state** st);

int ebur128_write_frames(ebur128_state* st, const double* src, size_t frames);
void ebur128_start_new_segment(ebur128_state* st);

double ebur128_gated_loudness_segment(ebur128_state* st);
double ebur128_gated_loudness_global(ebur128_state* st);

#ifdef __cplusplus
}
#endif

#endif  /* _EBUR128_H_ */

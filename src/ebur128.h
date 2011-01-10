#ifndef _EBUR128_H_
#define _EBUR128_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include <sys/queue.h>

TAILQ_HEAD(ebur128_double_queue, ebur128_dq_entry);
struct ebur128_dq_entry {
  double z;
  TAILQ_ENTRY(ebur128_dq_entry) entries;
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
} ebur128_state;

ebur128_state* ebur128_init(int channels, int samplerate);
int ebur128_destroy(ebur128_state** st);

int ebur128_write_frames(ebur128_state* st, const double* src, size_t frames);

double ebur128_gated_loudness(ebur128_state* st);

#ifdef __cplusplus
}
#endif

#endif  /* _EBUR128_H_ */

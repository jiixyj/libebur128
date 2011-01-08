#include <string.h>

typedef struct {
  double* audio_data;
  size_t audio_data_index;
  size_t frames;
  size_t blocks;
  size_t channels;
  double** v;
  double** v2;
  double* z;
  double** zg;
  size_t zg_index;
  double* lg;
} ebur128_state;

ebur128_state* ebur128_init(size_t frames, int channels);
int ebur128_destroy(ebur128_state** st);

int ebur128_write_frames(ebur128_state* st, const double* src, size_t frames);

double ebur128_relative_threshold(ebur128_state* st);
double ebur128_gated_loudness(ebur128_state* st, double relative_threshold);

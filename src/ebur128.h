#include <stdlib.h>

typedef struct {
  double* audio_data;
  size_t audio_data_index;
  size_t frames;
  int channels;
  int audio_data_half;
  double** v;
  double** v2;
  double* z;
  double** zg;
  size_t zg_index;
  double loudness;
  double* lg;
} ebur128_state;

int ebur128_init(ebur128_state* state, size_t frames, int channels);

int filter(double* dest, const double* source,
           size_t frames, int channels, int c,
           const double* b,
           const double* a,
           double** v);


int ebur128_do_stuff(ebur128_state* state, size_t frames);

int init_filter_state(double*** v, int channels, int filter_size);


void release_filter_state(double*** v, int channels);

void calc_gating_block(double* audio_data, size_t nr_frames_read,
                       int channels,
                       double** zg, size_t zg_index);

void calculate_block_loudness(double* lg, double** zg,
                              size_t frames, int channels);

void calculate_relative_threshold(double* relative_threshold,
                                  double* lg, double** zg,
                                  size_t frames, int channels);


void calculate_gated_loudness(double* gated_loudness,
                              double relative_threshold,
                              double* lg, double** zg,
                              size_t frames, int channels);

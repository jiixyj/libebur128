#include <stdlib.h>

int filter(double* dest, const double* source,
           size_t frames, int channels, int c,
           const double* b,
           const double* a,
           double** v);


int do_stuff(double* audio_data, size_t frames, int channels,
             double** v, double** v2,
             double* z);

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

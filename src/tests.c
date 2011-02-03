/* See LICENSE file for copyright and license details. */
#include <math.h>
#include <sndfile.h>
#include <string.h>
#include <stdlib.h>

#include "./ebur128.h"

double test_global_loudness(const char* filename) {
  SF_INFO file_info;
  SNDFILE* file;
  sf_count_t nr_frames_read;

  ebur128_state* st = NULL;
  double gated_loudness;
  double* buffer;

  memset(&file_info, '\0', sizeof(file_info));
  file = sf_open(filename, SFM_READ, &file_info);
  if (!file) {
    fprintf(stderr, "Could not open file %s!\n", filename);
    return 0.0 / 0.0;
  }
  st = ebur128_init(file_info.channels,
                    file_info.samplerate,
                    EBUR128_MODE_I);
  if (file_info.channels == 5) {
    int channel_map_five[] = {EBUR128_LEFT,
                              EBUR128_RIGHT,
                              EBUR128_CENTER,
                              EBUR128_LEFT_SURROUND,
                              EBUR128_RIGHT_SURROUND};
    ebur128_set_channel_map(st, channel_map_five);
  }
  buffer = (double*) malloc(st->samplerate * st->channels * sizeof(double));
  while ((nr_frames_read = sf_readf_double(file, buffer,
                                           (sf_count_t) st->samplerate))) {
    ebur128_add_frames_double(st, buffer, (size_t) nr_frames_read);
  }

  gated_loudness = ebur128_loudness_global(st);

  /* clean up */
  ebur128_destroy(&st);

  free(buffer);
  buffer = NULL;
  if (sf_close(file)) {
    fprintf(stderr, "Could not close input file!\n");
  }
  return gated_loudness;
}

double test_loudness_range(const char* filename) {
  SF_INFO file_info;
  SNDFILE* file;
  sf_count_t nr_frames_read;

  ebur128_state* st = NULL;
  double loudness_range;
  double* buffer;

  memset(&file_info, '\0', sizeof(file_info));
  file = sf_open(filename, SFM_READ, &file_info);
  if (!file) {
    fprintf(stderr, "Could not open file %s!\n", filename);
    return 0.0 / 0.0;
  }
  st = ebur128_init(file_info.channels,
                    file_info.samplerate,
                    EBUR128_MODE_LRA);
  if (file_info.channels == 5) {
    int channel_map_five[] = {EBUR128_LEFT,
                              EBUR128_RIGHT,
                              EBUR128_CENTER,
                              EBUR128_LEFT_SURROUND,
                              EBUR128_RIGHT_SURROUND};
    ebur128_set_channel_map(st, channel_map_five);
  }
  buffer = (double*) malloc(st->samplerate * st->channels * sizeof(double));
  while ((nr_frames_read = sf_readf_double(file, buffer,
                                           (sf_count_t) st->samplerate))) {
    ebur128_add_frames_double(st, buffer, (size_t) nr_frames_read);
  }

  loudness_range = ebur128_loudness_range(st);

  /* clean up */
  ebur128_destroy(&st);

  free(buffer);
  buffer = NULL;
  if (sf_close(file)) {
    fprintf(stderr, "Could not close input file!\n");
  }
  return loudness_range;
}

double gr[] = {-23.0,
               -33.0,
               -23.0,
               -23.0,
               -23.0,
               -23.0,
               -23.0,
               -23.0,
               -23.0};
double gre[] = {-2.2953556442089987e+01,
                -3.2959860397340044e+01,
                -2.2995899818255047e+01,
                -2.3035918615414182e+01,
                -2.2949997446096436e+01,
                -2.3017157781104373e+01,
                -2.3017157781104373e+01,
                -2.2980242495081757e+01,
                -2.3009077718930545e+01};
double lra[] = {10.0,
                 5.0,
                20.0,
                15.0,
                 5.0,
                15.0};
double lrae[] = {1.0001105488329134e+01,
                 4.9993734051522178e+00,
                 1.9995064067783115e+01,
                 1.4999273937723455e+01,
                 4.9747585878473721e+00,
                 1.4993650849123316e+01};


int main() {
  double result;

  fprintf(stderr, "%s\n",
                  "Note: the tests do not have to pass with EXACT_PASSED.\n"
                  "Passing these tests does not mean that the library is "
                  "100%% EBU R 128 compliant!\n");

#define TEST_GLOBAL_LOUDNESS(filename, i)                                      \
  result = test_global_loudness(filename);                                     \
  if (result == result) {                                                      \
    printf("%s, %s - %s: %1.16e\n",                                            \
       (result <= gr[i] + 0.1 && result >= gr[i] - 0.1) ? "PASSED" : "FAILED", \
       (result == gre[i]) ?  "EXACT_PASSED" : "EXACT_FAILED",                  \
       filename, result);                                                      \
  }

  TEST_GLOBAL_LOUDNESS("seq-3341-1-16bit.wav", 0)
  TEST_GLOBAL_LOUDNESS("seq-3341-2-16bit.wav", 1)
  TEST_GLOBAL_LOUDNESS("seq-3341-3-16bit.wav", 2)
  TEST_GLOBAL_LOUDNESS("seq-3341-4-16bit.wav", 3)
  TEST_GLOBAL_LOUDNESS("seq-3341-5-16bit.wav", 4)
  TEST_GLOBAL_LOUDNESS("seq-3341-6-5channels-16bit.wav", 5)
  TEST_GLOBAL_LOUDNESS("seq-3341-6-6channels-WAVEEX-16bit.wav", 6)
  TEST_GLOBAL_LOUDNESS("seq-3341-7_seq-3342-5-24bit.wav", 7)
  TEST_GLOBAL_LOUDNESS("seq-3341-8_seq-3342-6-24bit.wav", 8)


#define TEST_LRA(filename, i)                                                  \
  result = test_loudness_range(filename);                                      \
  if (result == result) {                                                      \
    printf("%s, %s - %s: %1.16e\n",                                            \
       (result <= lra[i] + 1 && result >= lra[i] - 1) ? "PASSED" : "FAILED",   \
       (result == lrae[i]) ?  "EXACT_PASSED" : "EXACT_FAILED",                 \
       filename, result);                                                      \
  }

  TEST_LRA("seq-3342-1-16bit.wav", 0)
  TEST_LRA("seq-3342-2-16bit.wav", 1)
  TEST_LRA("seq-3342-3-16bit.wav", 2)
  TEST_LRA("seq-3342-4-16bit.wav", 3)
  TEST_LRA("seq-3341-7_seq-3342-5-24bit.wav", 4)
  TEST_LRA("seq-3341-8_seq-3342-6-24bit.wav", 5)

  return 0;
}

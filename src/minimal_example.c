/* See LICENSE file for copyright and license details. */
#include <math.h>
#include <sndfile.h>
#include <string.h>
#include <stdlib.h>

#include "./ebur128.h"


int main(int ac, const char* av[]) {
  SF_INFO file_info;
  SNDFILE* file;
  sf_count_t nr_frames_read;
  sf_count_t nr_frames_read_all = 0;
  ebur128_state* st = NULL;
  double* buffer;
  double gated_loudness;

  if (ac < 2) {
    fprintf(stderr, "usage: r128-test FILENAME\n");
    exit(1);
  }

  memset(&file_info, '\0', sizeof(file_info));
  file = sf_open(av[1], SFM_READ, &file_info);


  st = ebur128_init(file_info.channels,
                    file_info.samplerate,
                    EBUR128_MODE_I);

  /* example: set channel map (note: see ebur128.h for the default map) */
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
    nr_frames_read_all += nr_frames_read;
    ebur128_add_frames_double(st, buffer, (size_t) nr_frames_read);
  }
  if (file_info.frames != nr_frames_read_all) {
    fprintf(stderr, "Warning: Could not read full file"
                            " or determine right length!\n");
  }

  gated_loudness = ebur128_gated_loudness_global(st);
  fprintf(stderr, "global loudness: %.1f LUFS\n", gated_loudness);


  /* clean up */
  ebur128_destroy(&st);

  free(buffer);
  buffer = NULL;

  if (sf_close(file)) {
    fprintf(stderr, "Could not close input file!\n");
  }

  return 0;
}

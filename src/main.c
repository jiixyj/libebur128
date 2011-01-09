#include <math.h>
#include <sndfile.h>
#include <string.h>
#include <stdlib.h>

#include "./ebur128.h"

#define CHECK_ERROR(condition, message, errorcode, goto_point)                 \
  if ((condition)) {                                                           \
    fprintf(stderr, message);                                                  \
    errcode = (errorcode);                                                     \
    goto goto_point;                                                           \
  }


int main(int ac, const char* av[]) {
  SF_INFO file_info;
  SNDFILE* file;
  sf_count_t nr_frames_read;
  sf_count_t nr_frames_read_all = 0;
  ebur128_state* st;
  double* buffer;
  double relative_threshold, gated_loudness;
  int errcode = 0;
  int result;

  CHECK_ERROR(ac != 2, "usage: r128-test FILENAME\n", 1, exit)

  memset(&file_info, '\0', sizeof(file_info));
  if (av[1][0] == '-' && av[1][1] == '\0') {
    file = sf_open_fd(0, SFM_READ, &file_info, SF_FALSE);
  } else {
    file = sf_open(av[1], SFM_READ, &file_info);
  }
  CHECK_ERROR(!file, "Could not open input file!\n", 1, exit)
  CHECK_ERROR(file_info.samplerate != 48000,
              "Only 48000 kHz sample rate is supported.\n", 1, close_file)

  st = ebur128_init( (size_t) file_info.frames,
                              file_info.channels,
                              file_info.samplerate);
  CHECK_ERROR(!st, "Could not initialize EBU R128!\n", 1, close_file)

  buffer = (double*) malloc(48000 * st->channels * sizeof(double));
  CHECK_ERROR(!buffer, "Could not allocate memory!\n", 1, destroy_ebur128)
  while ((nr_frames_read = sf_readf_double(file, buffer, 48000))) {
    nr_frames_read_all += nr_frames_read;
    result = ebur128_write_frames(st, buffer, (size_t) nr_frames_read);
    CHECK_ERROR(result, "Internal EBU R128 error!\n", 1, free_buffer)
  }
  if (file_info.frames != nr_frames_read_all) {
    fprintf(stderr, "Warning: Could not read full file"
                            " or determine right length!\n");
  }

  relative_threshold = ebur128_relative_threshold(st);
  gated_loudness = ebur128_gated_loudness(st, relative_threshold);

  fprintf(stderr, "relative threshold: %f LKFS\n", relative_threshold);
  fprintf(stderr, "gated loudness: %f LKFS\n", gated_loudness);

free_buffer:
  free(buffer);
  buffer = NULL;

destroy_ebur128:
  ebur128_destroy(&st);

close_file:
  if (sf_close(file)) {
    fprintf(stderr, "Could not close input file!\n");
  }

exit:
  return errcode;
}

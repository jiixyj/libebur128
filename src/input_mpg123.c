#include <mpg123.h>
#include <stdio.h>

#include "./ebur128.h"
#include "./common.h"

int init_input_library() {
  int result = mpg123_init();
  if (result != MPG123_OK) {
    return 1;
  }
  return 0;
}

void exit_input_library() {
  mpg123_exit();
}

void calculate_gain_of_file(void* user, void* user_data) {
  struct gain_data* gd = (struct gain_data*) user_data;
  size_t i = (size_t) (long) user - 1;
  char* const* av = gd->file_names;
  double* segment_loudness = gd->segment_loudness;
  double* segment_peaks = gd->segment_peaks;
  int calculate_lra = gd->calculate_lra, tag_rg = gd->tag_rg;

  mpg123_handle* mh = NULL;
  long mh_rate;
  int mh_channels, mh_encoding;
  float* buffer;

  ebur128_state* st = NULL;

  int errcode, result;

  segment_loudness[i] = -1.0 / 0.0;
  mh = mpg123_new(NULL, &result);
  CHECK_ERROR(!mh, "Could not create mpg123 handler!\n", 1, close_file)
  result = mpg123_open(mh, av[i]);
  CHECK_ERROR(result != MPG123_OK, "Could not open input file!\n", 1,
                                   close_file)
  result = mpg123_getformat(mh, &mh_rate, &mh_channels, &mh_encoding);
  CHECK_ERROR(result != MPG123_OK, "mpg123_getformat failed!\n", 1,
                                   close_file)
  result = mpg123_format_none(mh);
  CHECK_ERROR(result != MPG123_OK, "mpg123_format_none failed!\n", 1,
                                   close_file)
  result = mpg123_format(mh, mh_rate, mh_channels, MPG123_ENC_FLOAT_32);
  CHECK_ERROR(result != MPG123_OK, "mpg123_format failed!\n", 1, close_file)
  result = mpg123_close(mh);
  result = mpg123_open(mh, av[i]);
  CHECK_ERROR(result != MPG123_OK, "Could not open input file!\n", 1,
                                   close_file)
  result = mpg123_getformat(mh, &mh_rate, &mh_channels, &mh_encoding);
  CHECK_ERROR(result != MPG123_OK, "mpg123_getformat failed!\n", 1,
                                   close_file)

  st = ebur128_init(mh_channels,
                    (int) mh_rate,
                    EBUR128_MODE_I |
                    (calculate_lra ? EBUR128_MODE_LRA : 0));
  CHECK_ERROR(!st, "Could not initialize EBU R128!\n", 1, close_file)
  gd->library_states[i] = st;

  buffer = (float*) malloc(st->samplerate * st->channels * sizeof(float));
  CHECK_ERROR(!buffer, "Could not allocate memory!\n", 1, close_file)
  segment_peaks[i] = 0.0;
  for (;;) {
    size_t nr_frames_read;
    result = mpg123_read(mh, (unsigned char*) buffer,
                             st->samplerate * st->channels * sizeof(float),
                             &nr_frames_read);
    if (result != MPG123_OK && result != MPG123_DONE) {
      if (result == MPG123_ERR && mpg123_errcode(mh) == MPG123_RESYNC_FAIL) {
        fprintf(stderr, "%s\n", mpg123_strerror(mh));
        fprintf(stderr, "Maybe your file has an APEv2 tag?\n");
        break;
      } else {
        fprintf(stderr, "Internal MPG123 error!\n");
        errcode = 1;
        goto free_buffer;
      }
    }
    nr_frames_read /= st->channels * sizeof(float);
    if (!nr_frames_read) break;
    if (tag_rg) {
      size_t j;
      for (j = 0; j < (size_t) nr_frames_read * st->channels; ++j) {
        if (buffer[j] > segment_peaks[i])
          segment_peaks[i] = buffer[j];
        else if (-buffer[j] > segment_peaks[i])
          segment_peaks[i] = -buffer[j];
      }
    }
    result = ebur128_add_frames_float(st, buffer, (size_t) nr_frames_read);
    CHECK_ERROR(result, "Internal EBU R128 error!\n", 1, free_buffer)
  }

  segment_loudness[i] = ebur128_loudness_global(st);
  fprintf(stderr, "*");

free_buffer:
  free(buffer);
  buffer = NULL;

close_file:
  mpg123_close(mh);
  mpg123_delete(mh);
  mh = NULL;
}

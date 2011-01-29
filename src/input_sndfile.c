/* See LICENSE file for copyright and license details. */
#include <sndfile.h>
#include <stdlib.h>

#include "./ebur128.h"

#include "./common.h"

int init_input_library() {
  return 0;
}

void exit_input_library() {
  return;
}

void calculate_gain_of_file(void* user, void* user_data) {
  struct gain_data* gd = (struct gain_data*) user_data;
  size_t i = (size_t) user - 1;
  char* const* av = gd->file_names;
  double* segment_loudness = gd->segment_loudness;
  double* segment_peaks = gd->segment_peaks;
  int calculate_lra = gd->calculate_lra, tag_rg = gd->tag_rg;

  SF_INFO file_info;
  SNDFILE* file;
  float* buffer;
  size_t nr_frames_read_all = 0;

  ebur128_state* st = NULL;

  int errcode, result;

  segment_loudness[i] = -1.0 / 0.0;
  memset(&file_info, '\0', sizeof(file_info));
  file = sf_open(av[i], SFM_READ, &file_info);
  CHECK_ERROR(!file, "Could not open file!\n", 1, endloop)

  st = ebur128_init(file_info.channels,
                    file_info.samplerate,
                    EBUR128_MODE_I |
                    (calculate_lra ? EBUR128_MODE_LRA : 0));
  CHECK_ERROR(!st, "Could not initialize EBU R128!\n", 1, close_file)
  gd->library_states[i] = st;

  result = sf_command(file, SFC_GET_CHANNEL_MAP_INFO,
                            (void*) st->channel_map,
                            (int) st->channels * (int) sizeof(int));
  /* If sndfile found a channel map, set it with
   * ebur128_set_channel_map */
  if (result == SF_TRUE) {
    int j;
    for (j = 0; j < (int) st->channels; ++j) {
      switch (st->channel_map[j]) {
        case SF_CHANNEL_MAP_INVALID:
          ebur128_set_channel(st, j, EBUR128_UNUSED);         break;
        case SF_CHANNEL_MAP_MONO:
          ebur128_set_channel(st, j, EBUR128_CENTER);         break;
        case SF_CHANNEL_MAP_LEFT:
          ebur128_set_channel(st, j, EBUR128_LEFT);           break;
        case SF_CHANNEL_MAP_RIGHT:
          ebur128_set_channel(st, j, EBUR128_RIGHT);          break;
        case SF_CHANNEL_MAP_CENTER:
          ebur128_set_channel(st, j, EBUR128_CENTER);         break;
        case SF_CHANNEL_MAP_REAR_LEFT:
          ebur128_set_channel(st, j, EBUR128_LEFT_SURROUND);  break;
        case SF_CHANNEL_MAP_REAR_RIGHT:
          ebur128_set_channel(st, j, EBUR128_RIGHT_SURROUND); break;
        default:
          ebur128_set_channel(st, j, EBUR128_UNUSED);         break;
      }
    }
  /* Special case seq-3341-6-5channels-16bit.wav.
   * Set channel map with function ebur128_set_channel. */
  } else if (st->channels == 5) {
    ebur128_set_channel(st, 0, EBUR128_LEFT);
    ebur128_set_channel(st, 1, EBUR128_RIGHT);
    ebur128_set_channel(st, 2, EBUR128_CENTER);
    ebur128_set_channel(st, 3, EBUR128_LEFT_SURROUND);
    ebur128_set_channel(st, 4, EBUR128_RIGHT_SURROUND);
  }

  buffer = (float*) malloc(st->samplerate * st->channels * sizeof(float));
  CHECK_ERROR(!buffer, "Could not allocate memory!\n", 1, close_file)
  segment_peaks[i] = 0.0;
  for (;;) {
    size_t nr_frames_read = (size_t) sf_readf_float(file, buffer,
                                                (sf_count_t) st->samplerate);
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
    nr_frames_read_all += nr_frames_read;
    result = ebur128_add_frames_float(st, buffer, (size_t) nr_frames_read);
    CHECK_ERROR(result, "Internal EBU R128 error!\n", 1, free_buffer)
  }
  if (file && (size_t) file_info.frames != nr_frames_read_all) {
    fprintf(stderr, "Warning: Could not read full file"
                            " or determine right length!\n");
  }

  segment_loudness[i] = ebur128_loudness_global(st);
  fprintf(stderr, "*");

free_buffer:
  free(buffer);
  buffer = NULL;

close_file:
  if (sf_close(file)) {
    fprintf(stderr, "Could not close input file!\n");
  }
  file = NULL;

endloop:
  gd->errcode = errcode;
}

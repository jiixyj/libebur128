#include <sndfile.h>

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
  size_t i = (size_t) (long) user - 1;
  char* const* av = gd->file_names;

  SF_INFO file_info;
  SNDFILE* file;
  float* buffer;
  ebur128_state* st = NULL;
  int channels, samplerate;
  size_t nr_frames_read, nr_frames_read_all;

  int errcode, result;

  gd->segment_loudness[i] = -1.0 / 0.0;
  memset(&file_info, '\0', sizeof(file_info));
  file = sf_open(av[i], SFM_READ, &file_info);
  CHECK_ERROR(!file, "Could not open file!\n", 1, endloop)
  channels = file_info.channels;
  samplerate = file_info.samplerate;
  nr_frames_read_all = 0;

  st = ebur128_init(channels,
                    samplerate,
                    EBUR128_MODE_I |
                    (gd->calculate_lra ? EBUR128_MODE_LRA : 0));
  CHECK_ERROR(!st, "Could not initialize EBU R128!\n", 1, close_file)
  gd->library_states[i] = st;

  result = sf_command(file, SFC_GET_CHANNEL_MAP_INFO,
                            (void*) st->channel_map,
                            channels * (int) sizeof(int));
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
  } else if (channels == 5) {
    ebur128_set_channel(st, 0, EBUR128_LEFT);
    ebur128_set_channel(st, 1, EBUR128_RIGHT);
    ebur128_set_channel(st, 2, EBUR128_CENTER);
    ebur128_set_channel(st, 3, EBUR128_LEFT_SURROUND);
    ebur128_set_channel(st, 4, EBUR128_RIGHT_SURROUND);
  }

  buffer = (float*) malloc(st->samplerate * st->channels * sizeof(float));
  CHECK_ERROR(!buffer, "Could not allocate memory!\n", 1, close_file)
  gd->segment_peaks[i] = 0.0;
  for (;;) {
    nr_frames_read = (size_t) sf_readf_float(file, buffer,
                                           (sf_count_t) st->samplerate);
    if (!nr_frames_read) break;
    if (gd->tag_rg) {
      size_t j;
      for (j = 0; j < (size_t) nr_frames_read * st->channels; ++j) {
        if (buffer[j] > gd->segment_peaks[i])
          gd->segment_peaks[i] = buffer[j];
        else if (-buffer[j] > gd->segment_peaks[i])
          gd->segment_peaks[i] = -buffer[j];
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

  gd->segment_loudness[i] = ebur128_loudness_global(st);
  fprintf(stderr, "*");

free_buffer:
  free(buffer);
  buffer = NULL;

close_file:
  if (sf_close(file)) {
    fprintf(stderr, "Could not close input file!\n");
  }
  file = NULL;

endloop: ;
}

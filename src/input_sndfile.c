/* See LICENSE file for copyright and license details. */
#include <sndfile.h>
#include <stdlib.h>

#include "ebur128.h"

struct input_handle {
  SF_INFO file_info;
  SNDFILE* file;
  float* buffer;
};

size_t input_get_channels(struct input_handle* ih) {
  return (size_t) ih->file_info.channels;
}

size_t input_get_samplerate(struct input_handle* ih) {
  return (size_t) ih->file_info.samplerate;
}

float* input_get_buffer(struct input_handle* ih) {
  return ih->buffer;
}

size_t input_get_buffer_size(struct input_handle* ih) {
  return (size_t) ih->file_info.samplerate *
         (size_t) ih->file_info.channels;
}

struct input_handle* input_handle_init() {
  struct input_handle* ret;
  ret = malloc(sizeof(struct input_handle));
  return ret;
}

void input_handle_destroy(struct input_handle** ih) {
  free(*ih);
  *ih = NULL;
}

int input_open_file(struct input_handle* ih, const char* filename) {
  memset(&ih->file_info, '\0', sizeof(ih->file_info));
  ih->file = sf_open(filename, SFM_READ, &ih->file_info);
  if (ih->file) {
    return 0;
  } else {
    return 1;
  }
}

int input_set_channel_map(struct input_handle* ih, ebur128_state* st) {
  int result = sf_command(ih->file, SFC_GET_CHANNEL_MAP_INFO,
                            (void*) st->channel_map,
                            (int) st->channels * (int) sizeof(int));
  /* If sndfile found a channel map, set it with
   * ebur128_set_channel_map */
  if (result == SF_TRUE) {
    size_t j;
    for (j = 0; j < st->channels; ++j) {
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
    return 0;
  } else {
    return 1;
  }
}

int input_allocate_buffer(struct input_handle* ih) {
  ih->buffer = (float*) malloc((size_t) ih->file_info.samplerate *
                               (size_t) ih->file_info.channels *
                               sizeof(float));
  if (ih->buffer) {
    return 0;
  } else {
    return 1;
  }
}

size_t input_read_frames(struct input_handle* ih) {
  return (size_t) sf_readf_float(ih->file, ih->buffer,
                                 (sf_count_t) ih->file_info.samplerate);
}

int input_check_ok(struct input_handle* ih, size_t nr_frames_read_all) {
  if (ih->file && (size_t) ih->file_info.frames != nr_frames_read_all) {
    return 1;
  } else {
    return 0;
  }
}

void input_free_buffer(struct input_handle* ih) {
  free(ih->buffer);
  ih->buffer = NULL;
}

void input_close_file(struct input_handle* ih) {
  if (sf_close(ih->file)) {
    fprintf(stderr, "Could not close input file!\n");
  }
  ih->file = NULL;
}

int input_init_library() {
  return 0;
}

void input_exit_library() {
  return;
}

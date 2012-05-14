/* See LICENSE file for copyright and license details. */
#define _POSIX_C_SOURCE 1
#include <sndfile.h>
#include <stdio.h>
#include <stdlib.h>
#include <gmodule.h>

#include "ebur128.h"
#include "input.h"

struct input_handle {
  SF_INFO file_info;
  SNDFILE* file;
  float* buffer;
};

static unsigned sndfile_get_channels(struct input_handle* ih) {
  return (unsigned) ih->file_info.channels;
}

static unsigned long sndfile_get_samplerate(struct input_handle* ih) {
  return (unsigned long) ih->file_info.samplerate;
}

static float* sndfile_get_buffer(struct input_handle* ih) {
  return ih->buffer;
}

static struct input_handle* sndfile_handle_init() {
  struct input_handle* ret;
  ret = malloc(sizeof(struct input_handle));
  memset(&ret->file_info, '\0', sizeof(ret->file_info));
  return ret;
}

static void sndfile_handle_destroy(struct input_handle** ih) {
  free(*ih);
  *ih = NULL;
}

static int sndfile_open_file(struct input_handle* ih, const char* filename) {
#ifdef G_OS_WIN32
  int fd;
  g_usleep(10);
  fd = input_open_fd(filename);
  if (fd < 0) return 1;
  ih->file = sf_open_fd(fd, SFM_READ, &ih->file_info, 1);
#else
  ih->file = sf_open(filename, SFM_READ, &ih->file_info);
#endif

  if (ih->file) {
    return 0;
  } else {
#ifdef G_OS_WIN32
    _close(fd);
#endif
    return 1;
  }
}

static int sndfile_set_channel_map(struct input_handle* ih, int* st) {
  int result;
  int* channel_map = (int*) calloc((size_t) ih->file_info.channels, sizeof(int));
  if (!channel_map) return 1;
  result = sf_command(ih->file, SFC_GET_CHANNEL_MAP_INFO,
                      (void*) channel_map,
                      (int) ((size_t) ih->file_info.channels * sizeof(int)));
  /* If sndfile found a channel map, set it with
   * ebur128_set_channel_map */
  if (result == SF_TRUE) {
    int j;
    for (j = 0; j < ih->file_info.channels; ++j) {
      switch (channel_map[j]) {
        case SF_CHANNEL_MAP_INVALID:
          st[j] = EBUR128_UNUSED;         break;
        case SF_CHANNEL_MAP_MONO:
          st[j] = EBUR128_CENTER;         break;
        case SF_CHANNEL_MAP_LEFT:
          st[j] = EBUR128_LEFT;           break;
        case SF_CHANNEL_MAP_RIGHT:
          st[j] = EBUR128_RIGHT;          break;
        case SF_CHANNEL_MAP_CENTER:
          st[j] = EBUR128_CENTER;         break;
        case SF_CHANNEL_MAP_REAR_LEFT:
          st[j] = EBUR128_LEFT_SURROUND;  break;
        case SF_CHANNEL_MAP_REAR_RIGHT:
          st[j] = EBUR128_RIGHT_SURROUND; break;
        default:
          st[j] = EBUR128_UNUSED;         break;
      }
    }
    free(channel_map);
    return 0;
  } else {
    free(channel_map);
    return 1;
  }
}

static int sndfile_allocate_buffer(struct input_handle* ih) {
  ih->buffer = (float*) malloc((size_t) ih->file_info.samplerate *
                               (size_t) ih->file_info.channels *
                               sizeof(float));
  if (ih->buffer) {
    return 0;
  } else {
    return 1;
  }
}

static size_t sndfile_get_total_frames(struct input_handle* ih) {
  return (size_t) ih->file_info.frames;
}

static size_t sndfile_read_frames(struct input_handle* ih) {
  return (size_t) sf_readf_float(ih->file, ih->buffer,
                                 (sf_count_t) ih->file_info.samplerate);
}

static void sndfile_free_buffer(struct input_handle* ih) {
  free(ih->buffer);
  ih->buffer = NULL;
}

static void sndfile_close_file(struct input_handle* ih) {
  if (sf_close(ih->file)) {
    fprintf(stderr, "Could not close input file!\n");
  }
  ih->file = NULL;
}

static int sndfile_init_library() {
  return 0;
}

static void sndfile_exit_library() {
  return;
}

G_MODULE_EXPORT struct input_ops ip_ops = {
  sndfile_get_channels,
  sndfile_get_samplerate,
  sndfile_get_buffer,
  sndfile_handle_init,
  sndfile_handle_destroy,
  sndfile_open_file,
  sndfile_set_channel_map,
  sndfile_allocate_buffer,
  sndfile_get_total_frames,
  sndfile_read_frames,
  sndfile_free_buffer,
  sndfile_close_file,
  sndfile_init_library,
  sndfile_exit_library
};

G_MODULE_EXPORT const char* ip_exts[] = {"wav", "flac", "ogg", "oga", NULL};

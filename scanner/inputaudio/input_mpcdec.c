/* See LICENSE file for copyright and license details. */
#include <mpc/mpcdec.h>
#include <gmodule.h>

#include "ebur128.h"
#include "input.h"

struct input_handle {
  mpc_reader reader;
  mpc_demux* demux;
  mpc_streaminfo si;
  mpc_status err;
  MPC_SAMPLE_FORMAT buffer[MPC_DECODER_BUFFER_LENGTH];
};

static unsigned mpcdec_get_channels(struct input_handle* ih) {
  return ih->si.channels;
}

static unsigned long mpcdec_get_samplerate(struct input_handle* ih) {
  return ih->si.sample_freq;
}

static float* mpcdec_get_buffer(struct input_handle* ih) {
  return ih->buffer;
}

static struct input_handle* mpcdec_handle_init() {
  struct input_handle* ret;
  ret = malloc(sizeof(struct input_handle));
  return ret;
}

static int mpcdec_open_file(struct input_handle* ih, const char* filename) {
#ifdef G_OS_WIN32
  int err;
  gunichar2 *utf16;
  FILE *file;

  utf16 = g_utf8_to_utf16(filename, -1, NULL, NULL, NULL);
  file = _wfopen(utf16, L"rb");
  g_free(utf16);
  if (!file) {
    return 1;
  }
  err = mpc_reader_init_stdio_stream(&ih->reader, file);
#else
  int err = mpc_reader_init_stdio(&ih->reader, filename);
#endif

  if (err < 0) {
    return 1;
  }
  ih->demux = mpc_demux_init(&ih->reader);
  if (!ih->demux) {
    return 1;
  }
  mpc_demux_get_info(ih->demux, &ih->si);
  return 0;
}

static int mpcdec_set_channel_map(struct input_handle* ih, int* st) {
  (void) ih;
  (void) st;
  return 1;
}

static void mpcdec_handle_destroy(struct input_handle** ih) {
  free(*ih);
  *ih = NULL;
}

static int mpcdec_allocate_buffer(struct input_handle* ih) {
  (void) ih;
  return 0;
}

static size_t mpcdec_get_total_frames(struct input_handle* ih) {
  return ih->si.samples;
}

static size_t mpcdec_read_frames(struct input_handle* ih) {
  mpc_frame_info frame;
  frame.buffer = ih->buffer;
  mpc_demux_decode(ih->demux, &frame);
  if (frame.bits == -1) return 0;

  return frame.samples;
}

static void mpcdec_free_buffer(struct input_handle* ih) {
  (void) ih;
  return;
}

static void mpcdec_close_file(struct input_handle* ih) {
  mpc_demux_exit(ih->demux);
  mpc_reader_exit_stdio(&ih->reader);
}

static int mpcdec_init_library() {
  return 0;
}

static void mpcdec_exit_library() {
  return;
}

G_MODULE_EXPORT struct input_ops ip_ops = {
  mpcdec_get_channels,
  mpcdec_get_samplerate,
  mpcdec_get_buffer,
  mpcdec_handle_init,
  mpcdec_handle_destroy,
  mpcdec_open_file,
  mpcdec_set_channel_map,
  mpcdec_allocate_buffer,
  mpcdec_get_total_frames,
  mpcdec_read_frames,
  mpcdec_free_buffer,
  mpcdec_close_file,
  mpcdec_init_library,
  mpcdec_exit_library
};

G_MODULE_EXPORT const char* ip_exts[] = {"mpc", NULL};

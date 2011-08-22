/* See LICENSE file for copyright and license details. */
#include <mpc/mpcdec.h>

#include "ebur128.h"
#include "input.h"

struct input_handle {
  mpc_reader reader;
  mpc_demux* demux;
  mpc_streaminfo si;
  mpc_status err;
  MPC_SAMPLE_FORMAT buffer[MPC_DECODER_BUFFER_LENGTH];
};

size_t mpcdec_get_channels(struct input_handle* ih) {
  return ih->si.channels;
}

size_t mpcdec_get_samplerate(struct input_handle* ih) {
  return ih->si.sample_freq;
}

float* mpcdec_get_buffer(struct input_handle* ih) {
  return ih->buffer;
}

size_t mpcdec_get_buffer_size(struct input_handle* ih) {
  (void) ih;
  return MPC_DECODER_BUFFER_LENGTH;
}

struct input_handle* mpcdec_handle_init() {
  struct input_handle* ret;
  ret = malloc(sizeof(struct input_handle));
  return ret;
}

int mpcdec_open_file(struct input_handle* ih, FILE* file) {
  int err = mpc_reader_init_stdio_stream(&ih->reader, file);
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

int mpcdec_set_channel_map(struct input_handle* ih, ebur128_state* st) {
  (void) ih;
  (void) st;
  return 1;
}

void mpcdec_handle_destroy(struct input_handle** ih) {
  free(*ih);
  *ih = NULL;
}

int mpcdec_allocate_buffer(struct input_handle* ih) {
  (void) ih;
  return 0;
}

size_t mpcdec_read_frames(struct input_handle* ih) {
  mpc_frame_info frame;
  frame.buffer = ih->buffer;
  mpc_demux_decode(ih->demux, &frame);
  if (frame.bits == -1) return 0;

  return frame.samples;
}

int mpcdec_check_ok(struct input_handle* ih, size_t nr_frames_read_all) {
  (void) ih;
  (void) nr_frames_read_all;
  return 0;
}

void mpcdec_free_buffer(struct input_handle* ih) {
  (void) ih;
  return;
}

void mpcdec_close_file(struct input_handle* ih, FILE* file) {
  (void) file;
  mpc_demux_exit(ih->demux);
  mpc_reader_exit_stdio(&ih->reader);
}

int mpcdec_init_library() {
  return 0;
}

void mpcdec_exit_library() {
  return;
}

R128EXPORT struct input_ops ip_ops = {
  mpcdec_get_channels,
  mpcdec_get_samplerate,
  mpcdec_get_buffer,
  mpcdec_get_buffer_size,
  mpcdec_handle_init,
  mpcdec_handle_destroy,
  mpcdec_open_file,
  mpcdec_set_channel_map,
  mpcdec_allocate_buffer,
  mpcdec_read_frames,
  mpcdec_check_ok,
  mpcdec_free_buffer,
  mpcdec_close_file,
  mpcdec_init_library,
  mpcdec_exit_library
};

R128EXPORT const char* ip_exts[] = {"mpc", NULL};

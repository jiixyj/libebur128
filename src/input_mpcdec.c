/* See LICENSE file for copyright and license details. */
#include <mpc/mpcdec.h>

#include "./ebur128.h"

struct input_handle {
  mpc_reader reader;
  mpc_demux* demux;
  mpc_streaminfo si;
  mpc_status err;
  MPC_SAMPLE_FORMAT buffer[MPC_DECODER_BUFFER_LENGTH];
};

int input_get_channels(struct input_handle* ih) {
  return (int) ih->si.channels;
}

int input_get_samplerate(struct input_handle* ih) {
  return (int) ih->si.sample_freq;
}

float* input_get_buffer(struct input_handle* ih) {
  return ih->buffer;
}

struct input_handle* input_handle_init() {
  struct input_handle* ret;
  ret = malloc(sizeof(struct input_handle));
  return ret;
}

int input_open_file(struct input_handle* ih, const char* filename) {
  int err = mpc_reader_init_stdio(&ih->reader, filename);
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

int input_set_channel_map(struct input_handle* ih, ebur128_state* st) {
  return 1;
}

void input_handle_destroy(struct input_handle** ih) {
  free(*ih);
  *ih = NULL;
}

int input_allocate_buffer(struct input_handle* ih) {
  return 0;
}

size_t input_read_frames(struct input_handle* ih) {
  mpc_frame_info frame;
  frame.buffer = ih->buffer;
  mpc_demux_decode(ih->demux, &frame);
  if (frame.bits == -1) return 0;

  return frame.samples;
}

int input_check_ok(struct input_handle* ih, size_t nr_frames_read_all) {
  return 0;
}

void input_free_buffer(struct input_handle* ih) {
  return;
}

void input_close_file(struct input_handle* ih) {
  mpc_demux_exit(ih->demux);
  mpc_reader_exit_stdio(&ih->reader);
}

int input_init_library() {
  return 0;
}

void input_exit_library() {
  return;
}

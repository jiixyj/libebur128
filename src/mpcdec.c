#include <mpc/mpcdec.h>

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
  double* segment_loudness = gd->segment_loudness;
  double* segment_peaks = gd->segment_peaks;
  int calculate_lra = gd->calculate_lra, tag_rg = gd->tag_rg;

  mpc_reader reader;
  mpc_demux* demux;
  mpc_streaminfo si;
  mpc_status err;
  MPC_SAMPLE_FORMAT buffer[MPC_DECODER_BUFFER_LENGTH];

  ebur128_state* st = NULL;
  int channels, samplerate;
  size_t nr_frames_read, nr_frames_read_all;

  int errcode, result;

  segment_loudness[i] = -1.0 / 0.0;
  err = mpc_reader_init_stdio(&reader, av[i]);
  if (err < 0) {
    fprintf(stderr, "Could not open file!\n");
    return;
  }
  demux = mpc_demux_init(&reader);
  CHECK_ERROR(!demux, "Could not initialize demuxer!\n", 1, reader_exit)
  mpc_demux_get_info(demux, &si);

  channels = (int) si.channels;
  samplerate = (int) si.sample_freq;
  nr_frames_read_all = 0;

  st = ebur128_init(channels,
                    samplerate,
                    EBUR128_MODE_I |
                    (calculate_lra ? EBUR128_MODE_LRA : 0));
  CHECK_ERROR(!st, "Could not initialize EBU R128!\n", 1, demux_exit)
  gd->library_states[i] = st;

  segment_peaks[i] = 0.0;
  for (;;) {
    mpc_frame_info frame;
    frame.buffer = buffer;
    err = mpc_demux_decode(demux, &frame);
    if (frame.bits == -1) break;

    nr_frames_read = frame.samples;
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
    CHECK_ERROR(result, "Internal EBU R128 error!\n", 1, demux_exit)
  }

  gd->segment_loudness[i] = ebur128_loudness_global(st);
  fprintf(stderr, "*");

demux_exit:
  mpc_demux_exit(demux);
reader_exit:
  mpc_reader_exit_stdio(&reader);
}

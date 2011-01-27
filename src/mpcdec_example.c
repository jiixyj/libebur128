/* See LICENSE file for copyright and license details. */
#define _POSIX_C_SOURCE 200112L
#include <float.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <mpc/mpcdec.h>

#include "./ebur128.h"

#define CHECK_ERROR(condition, message, errorcode, goto_point)                 \
  if ((condition)) {                                                           \
    fprintf(stderr, message);                                                  \
    errcode = (errorcode);                                                     \
    goto goto_point;                                                           \
  }


int main(int ac, char* const av[]) {
  mpc_reader reader;
  mpc_demux* demux;
  mpc_streaminfo si;
  mpc_status err;
  MPC_SAMPLE_FORMAT buffer[MPC_DECODER_BUFFER_LENGTH];

  int channels, samplerate;
  size_t nr_frames_read, nr_frames_read_all;

  ebur128_state* st = NULL;
  double gated_loudness = DBL_MAX;
  double* segment_loudness;
  double* segment_peaks;
  int calculate_lra = 0;
  int rgtag_info = 0;

  int errcode = 0;
  int result;
  int i;
  int c;

  CHECK_ERROR(ac < 2, "usage: r128-test [-r] [-t] FILENAME(S) ...\n\n"
                      " -r: calculate loudness range in LRA\n"
                      " -t: output ReplayGain tagging info\n", 1, exit)
  while ((c = getopt(ac, av, "tr")) != -1) {
    switch (c) {
      case 't':
        rgtag_info = 1;
        break;
      case 'r':
        calculate_lra = 1;
        break;
      default:
        return 1;
        break;
    }
  }

  segment_loudness = calloc((size_t) (ac - optind), sizeof(double));
  segment_peaks = calloc((size_t) (ac - optind), sizeof(double));

  for (i = optind; i < ac; ++i) {
    segment_loudness[i - optind] = DBL_MAX;
    err = mpc_reader_init_stdio(&reader, av[i]);
    if (err < 0) {
      fprintf(stderr, "Could not open file!\n");
      continue;
    }
    demux = mpc_demux_init(&reader);
    CHECK_ERROR(!demux, "Could not initialize demuxer!\n", 1, reader_exit)
    mpc_demux_get_info(demux, &si);

    channels = (int) si.channels;
    samplerate = (int) si.sample_freq;
    nr_frames_read_all = 0;

    if (!st) {
      st = ebur128_init(channels,
                        samplerate,
                        EBUR128_MODE_I |
                        (calculate_lra ? EBUR128_MODE_LRA : 0));
      CHECK_ERROR(!st, "Could not initialize EBU R128!\n", 1, demux_exit)
    } else {
      CHECK_ERROR(st->channels != (size_t) channels ||
                  st->samplerate != (size_t) samplerate,
                  "All files must have the same samplerate "
                  "and number of channels! Skipping...\n",
                  1, demux_exit)
    }

    segment_peaks[i - optind] = 0.0;
    for (;;) {
      mpc_frame_info frame;
      frame.buffer = buffer;
      err = mpc_demux_decode(demux, &frame);
      if (frame.bits == -1) break;

      nr_frames_read = frame.samples;
      if (!nr_frames_read) break;
      if (rgtag_info) {
        size_t j;
        for (j = 0; j < (size_t) nr_frames_read * st->channels; ++j) {
          if (buffer[j] > segment_peaks[i - optind])
            segment_peaks[i - optind] = buffer[j];
          else if (-buffer[j] > segment_peaks[i - optind])
            segment_peaks[i - optind] = -buffer[j];
        }
      }
      result = ebur128_add_frames_float(st, buffer, (size_t) nr_frames_read);
      CHECK_ERROR(result, "Internal EBU R128 error!\n", 1, demux_exit)
    }

    segment_loudness[i - optind] = ebur128_loudness_segment(st);
    if (ac - optind != 1) {
      fprintf(stderr, "segment %d: %.2f LUFS\n", i + 1 - optind,
                      segment_loudness[i - optind]);
      ebur128_start_new_segment(st);
    }
    if (i == ac - 1) {
      gated_loudness = ebur128_loudness_global(st);
      fprintf(stderr, "global loudness: %.2f LUFS\n", gated_loudness);
    }

  demux_exit:
    mpc_demux_exit(demux);
  reader_exit:
    mpc_reader_exit_stdio(&reader);
  }

  if (st && calculate_lra) {
    fprintf(stderr, "LRA: %.2f\n", ebur128_loudness_range(st));
  }

  if (st && rgtag_info) {
    double global_peak = 0.0;
    for (i = 0; i < ac - optind; ++i) {
      if (segment_peaks[i] > global_peak) {
        global_peak = segment_peaks[i];
      }
    }
    for (i = optind; i < ac; ++i) {
      printf("%.8f %.8f %.8f %.8f\n", -18.0 - segment_loudness[i - optind],
                                      segment_peaks[i - optind],
                                      -18.0 - gated_loudness,
                                      global_peak);
    }
  }

  if (st)
    ebur128_destroy(&st);
  if (segment_loudness)
    free(segment_loudness);
  if (segment_peaks)
    free(segment_peaks);

exit:
  return errcode;
}

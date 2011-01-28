/* See LICENSE file for copyright and license details. */
#define _POSIX_C_SOURCE 200112L
#include <float.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <sndfile.h>

#include "./ebur128.h"

#define CHECK_ERROR(condition, message, errorcode, goto_point)                 \
  if ((condition)) {                                                           \
    fprintf(stderr, message);                                                  \
    errcode = (errorcode);                                                     \
    goto goto_point;                                                           \
  }


int main(int ac, char* const av[]) {
  SF_INFO file_info;
  SNDFILE* file;

  int channels, samplerate;
  size_t nr_frames_read, nr_frames_read_all;

  float* buffer;

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
    memset(&file_info, '\0', sizeof(file_info));
    if (av[i][0] == '-' && av[1][1] == '\0') {
      file = sf_open_fd(0, SFM_READ, &file_info, SF_FALSE);
    } else {
      file = sf_open(av[i], SFM_READ, &file_info);
    }
    CHECK_ERROR(!file, "Could not open file!\n", 1, endloop)
    channels = file_info.channels;
    samplerate = file_info.samplerate;
    nr_frames_read_all = 0;

    if (!st) {
      st = ebur128_init(channels,
                        samplerate,
                        EBUR128_MODE_I |
                        (calculate_lra ? EBUR128_MODE_LRA : 0));
      CHECK_ERROR(!st, "Could not initialize EBU R128!\n", 1, close_file)

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
    } else {
      CHECK_ERROR(st->channels != (size_t) channels ||
                  st->samplerate != (size_t) samplerate,
                  "All files must have the same samplerate "
                  "and number of channels! Skipping...\n",
                  1, close_file)
    }

    buffer = (float*) malloc(st->samplerate * st->channels * sizeof(float));
    CHECK_ERROR(!buffer, "Could not allocate memory!\n", 1, close_file)
    segment_peaks[i - optind] = 0.0;
    for (;;) {
      nr_frames_read = (size_t) sf_readf_float(file, buffer,
                                             (sf_count_t) st->samplerate);
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
      nr_frames_read_all += nr_frames_read;
      result = ebur128_add_frames_float(st, buffer, (size_t) nr_frames_read);
      CHECK_ERROR(result, "Internal EBU R128 error!\n", 1, free_buffer)
    }
    if (file && (size_t) file_info.frames != nr_frames_read_all) {
      fprintf(stderr, "Warning: Could not read full file"
                              " or determine right length!\n");
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

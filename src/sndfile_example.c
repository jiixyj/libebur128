/* See LICENSE file for copyright and license details. */
#include <math.h>
#include <sndfile.h>
#include <string.h>
#include <stdlib.h>

#include "./ebur128.h"

#define CHECK_ERROR(condition, message, errorcode, goto_point)                 \
  if ((condition)) {                                                           \
    fprintf(stderr, message);                                                  \
    errcode = (errorcode);                                                     \
    goto goto_point;                                                           \
  }


int main(int ac, const char* av[]) {
  SF_INFO file_info;
  SNDFILE* file;
  sf_count_t nr_frames_read;
  sf_count_t nr_frames_read_all;
  ebur128_state* st = NULL;
  short* buffer;
  double gated_loudness;
  int errcode = 0;
  int result;
  int i;

  CHECK_ERROR(ac < 2, "usage: r128-test FILENAME(S) ...\n", 1, exit)

  for (i = 1; i < ac; ++i) {
    memset(&file_info, '\0', sizeof(file_info));
    if (av[1][0] == '-' && av[1][1] == '\0') {
      file = sf_open_fd(0, SFM_READ, &file_info, SF_FALSE);
    } else {
      file = sf_open(av[i], SFM_READ, &file_info);
    }
    CHECK_ERROR(!file, "Could not open input file!\n", 1, endloop)
    nr_frames_read_all = 0;


    if (!st) {
      st = ebur128_init(file_info.channels,
                        file_info.samplerate,
                        EBUR128_MODE_M_I);
      CHECK_ERROR(!st, "Could not initialize EBU R128!\n", 1, close_file)

      result = sf_command(file, SFC_GET_CHANNEL_MAP_INFO,
                                (void*) st->channel_map,
                                file_info.channels * (int) sizeof(int));
      /* If sndfile found a channel map, set it directly to st->channel_map */
      if (result == SF_TRUE) {
        size_t j;
        for (j = 0; j < st->channels; ++j) {
          switch (st->channel_map[j]) {
            case SF_CHANNEL_MAP_INVALID:
              st->channel_map[j] = EBUR128_UNUSED;         break;
            case SF_CHANNEL_MAP_MONO:
              st->channel_map[j] = EBUR128_CENTER;         break;
            case SF_CHANNEL_MAP_LEFT:
              st->channel_map[j] = EBUR128_LEFT;           break;
            case SF_CHANNEL_MAP_RIGHT:
              st->channel_map[j] = EBUR128_RIGHT;          break;
            case SF_CHANNEL_MAP_CENTER:
              st->channel_map[j] = EBUR128_CENTER;         break;
            case SF_CHANNEL_MAP_REAR_LEFT:
              st->channel_map[j] = EBUR128_LEFT_SURROUND;  break;
            case SF_CHANNEL_MAP_REAR_RIGHT:
              st->channel_map[j] = EBUR128_RIGHT_SURROUND; break;
            default:
              st->channel_map[j] = EBUR128_UNUSED; break;
          }
        }
      /* Special case seq-3341-6-5channels-16bit.wav.
       * Set channel map with function ebur128_set_channel_map. */
      } else if (file_info.channels == 5) {
        int channel_map_five[] = {EBUR128_LEFT,
                                  EBUR128_RIGHT,
                                  EBUR128_CENTER,
                                  EBUR128_LEFT_SURROUND,
                                  EBUR128_RIGHT_SURROUND};
        ebur128_set_channel_map(st, channel_map_five);
      }
    } else {
      CHECK_ERROR(st->channels != (size_t) file_info.channels ||
                  st->samplerate != (size_t) file_info.samplerate,
                  "All files must have the same samplerate "
                  "and number of channels! Skipping...\n",
                  1, close_file)
    }

    buffer = (short*) malloc(st->samplerate * st->channels * sizeof(short));
    CHECK_ERROR(!buffer, "Could not allocate memory!\n", 1, close_file)
    while ((nr_frames_read = sf_readf_short(file, buffer,
                                             (sf_count_t) st->samplerate))) {
      nr_frames_read_all += nr_frames_read;
      result = ebur128_get_frames_short(st, buffer, (size_t) nr_frames_read);
      CHECK_ERROR(result, "Internal EBU R128 error!\n", 1, free_buffer)
      /* printf("%f\n", ebur128_loudness_shortterm(st)); */
    }
    if (file_info.frames != nr_frames_read_all) {
      fprintf(stderr, "Warning: Could not read full file"
                              " or determine right length!\n");
    }

    if (ac != 2) {
      fprintf(stderr, "segment %d: %.1f LUFS\n", i,
                      ebur128_gated_loudness_segment(st));
      ebur128_start_new_segment(st);
    }
    if (i == ac - 1) {
      gated_loudness = ebur128_gated_loudness_global(st);
      fprintf(stderr, "global loudness: %.1f LUFS\n", gated_loudness);
    }

  free_buffer:
    free(buffer);
    buffer = NULL;

  close_file:
    if (sf_close(file)) {
      fprintf(stderr, "Could not close input file!\n");
    }

  endloop: ;
  }

  if (st)
    ebur128_destroy(&st);

exit:
  return errcode;
}

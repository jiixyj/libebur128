/* See LICENSE file for copyright and license details. */
#define _POSIX_C_SOURCE 200112L
#include <float.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include "./ebur128.h"

#define CHECK_ERROR(condition, message, errorcode, goto_point)                 \
  if ((condition)) {                                                           \
    fprintf(stderr, message);                                                  \
    errcode = (errorcode);                                                     \
    goto goto_point;                                                           \
  }


int main(int ac, char* const av[]) {
  AVFormatContext* format_context;
  AVCodecContext* codec_context;
  AVCodec* codec;
  AVPacket packet;

  ebur128_state* st = NULL;
  double gated_loudness = DBL_MAX;
  int calculate_lra = 0;

  int errcode = 0;
  int i;
  char* rgtag_exe = NULL;
  int c;

  int result;

  CHECK_ERROR(ac < 2, "usage: r128-test [-r] [-t RGTAG_EXE] FILENAME(S) ...\n\n"
                      " -r: calculate loudness range in LRA\n"
                      " -t: specify ReplayGain tagging script\n", 1, exit)
  while ((c = getopt(ac, av, "t:r")) != -1) {
    switch (c) {
      case 't':
        rgtag_exe = optarg;
        break;
      case 'r':
        calculate_lra = 1;
        break;
      default:
        return 1;
        break;
    }
  }

  // Register all formats and codecs
  av_register_all();
  av_log_set_level(AV_LOG_ERROR);

  double* segment_loudness = calloc((size_t) (ac - optind), sizeof(double));
  double* segment_peaks = calloc((size_t) (ac - optind), sizeof(double));
  for (i = optind; i < ac; ++i) {
    segment_loudness[i - optind] = DBL_MAX;
    if (av_open_input_file(&format_context, av[i], NULL, 0, NULL) != 0) {
      fprintf(stderr, "Could not open input file!\n");
      continue;
    }
    if (av_find_stream_info(format_context) < 0) {
      fprintf(stderr, "Could not find stream info!\n");
      goto close_file;
    }
    // Dump information about file onto standard error
    // dump_format(format_context, 0, av[1], 0);

    // Find the first audio stream
    int audio_stream = -1;
    for (int j = 0; (unsigned) j < format_context->nb_streams; ++j) {
      if (format_context->streams[j]->codec->codec_type == CODEC_TYPE_AUDIO) {
        audio_stream = j;
        break;
      }
    }
    if (audio_stream == -1) {
      fprintf(stderr, "Could not find an audio stream in file!\n");
      goto close_file;
    }
    // Get a pointer to the codec context for the audio stream
    codec_context = format_context->streams[audio_stream]->codec;
    // Find the decoder for the video stream
    codec = avcodec_find_decoder(codec_context->codec_id);
    if (codec == NULL) {
      fprintf(stderr, "Could not find a decoder for the audio format!\n");
      goto close_file;
    }
    // Open codec
    if (avcodec_open(codec_context, codec) < 0) {
      fprintf(stderr, "Could not open the codec!\n");
      goto close_file;
    }


    if (!st) {
      st = ebur128_init(codec_context->channels,
                        codec_context->sample_rate,
                        EBUR128_MODE_I |
                        (calculate_lra ? EBUR128_MODE_LRA : 0));
      CHECK_ERROR(!st, "Could not initialize EBU R128!\n", 1, close_codec)

      if (codec_context->channel_layout) {
        int channel_map_index = 0;
        int bit_counter = 0;
        while (channel_map_index < codec_context->channels) {
          if (codec_context->channel_layout & (1 << bit_counter)) {
            switch (1 << bit_counter) {
              case CH_FRONT_LEFT:
                st->channel_map[channel_map_index] = EBUR128_LEFT;
                break;
              case CH_FRONT_RIGHT:
                st->channel_map[channel_map_index] = EBUR128_RIGHT;
                break;
              case CH_FRONT_CENTER:
                st->channel_map[channel_map_index] = EBUR128_CENTER;
                break;
              case CH_BACK_LEFT:
                st->channel_map[channel_map_index] = EBUR128_LEFT_SURROUND;
                break;
              case CH_BACK_RIGHT:
                st->channel_map[channel_map_index] = EBUR128_RIGHT_SURROUND;
                break;
              default:
                st->channel_map[channel_map_index] = EBUR128_UNUSED;
                break;
            }
            ++channel_map_index;
          }
          ++bit_counter;
        }
      } else if (codec_context->channels == 5) {
        /* Special case seq-3341-6-5channels-16bit.wav.
         * Set channel map with function ebur128_set_channel_map. */
        int channel_map_five[] = {EBUR128_LEFT,
                                  EBUR128_RIGHT,
                                  EBUR128_CENTER,
                                  EBUR128_LEFT_SURROUND,
                                  EBUR128_RIGHT_SURROUND};
        ebur128_set_channel_map(st, channel_map_five);
      }
    } else {
      CHECK_ERROR(st->channels != (size_t) codec_context->channels ||
                  st->samplerate != (size_t) codec_context->sample_rate,
                  "All files must have the same samplerate "
                  "and number of channels! Skipping...\n",
                  1, close_codec)
    }


    while (av_read_frame(format_context, &packet) >= 0) {
      if (packet.stream_index == audio_stream) {
        uint8_t audio_buf[AVCODEC_MAX_AUDIO_FRAME_SIZE];
        int16_t* data_short =  (int16_t*) audio_buf;
        int32_t* data_int =    (int32_t*) audio_buf;
        float*   data_float =  (float*)   audio_buf;
        double*  data_double = (double*)  audio_buf;

        uint8_t* old_data = packet.data;
        while (packet.size > 0) {
          int data_size = sizeof(audio_buf);
#if LIBAVCODEC_VERSION_MAJOR >= 52 &&  \
    LIBAVCODEC_VERSION_MINOR >= 26 &&  \
    LIBAVCODEC_VERSION_MICRO >= 0
          int len = avcodec_decode_audio3(codec_context, (int16_t*) audio_buf, &data_size, &packet);
#else
          int len = avcodec_decode_audio2(codec_context, (int16_t*) audio_buf, &data_size, packet.data, packet.size);
#endif
          if (len < 0) {
            packet.size = 0;
            break;
          }
      #define CHECK_FOR_PEAKS(buffer, min_scale, max_scale)                    \
          if (rgtag_exe) {                                                     \
            double scale_factor = -min_scale > max_scale ? -min_scale          \
                                                         : max_scale;          \
            size_t j;                                                          \
            double buffer_scaled;                                              \
            for (j = 0; j < (size_t) nr_frames_read * st->channels; ++j) {     \
              buffer_scaled = buffer[j] / (scale_factor);                      \
              if (buffer_scaled > segment_peaks[i - optind])                   \
                segment_peaks[i - optind] = buffer_scaled;                     \
              else if (-buffer_scaled > segment_peaks[i - optind])             \
                segment_peaks[i - optind] = -buffer_scaled;                    \
            }                                                                  \
          }
          size_t nr_frames_read;
          switch (codec_context->sample_fmt) {
            case SAMPLE_FMT_U8:
              CHECK_ERROR(1, "8 bit audio not supported by libebur128!\n", 1, close_codec)
              break;
            case SAMPLE_FMT_S16:
              nr_frames_read = (size_t) data_size / sizeof(int16_t) /
                               (size_t) codec_context->channels;
              CHECK_FOR_PEAKS(data_short, SHRT_MIN, SHRT_MAX)
              result = ebur128_add_frames_short(st, data_short, nr_frames_read);
              CHECK_ERROR(result, "Internal EBU R128 error!\n", 1, close_codec)
              break;
            case SAMPLE_FMT_S32:
              nr_frames_read = (size_t) data_size / sizeof(int32_t) /
                               (size_t) codec_context->channels;
              CHECK_FOR_PEAKS(data_int, (long) INT_MIN, (long) INT_MAX)
              result = ebur128_add_frames_int(st, data_int, nr_frames_read);
              CHECK_ERROR(result, "Internal EBU R128 error!\n", 1, close_codec)
              break;
            case SAMPLE_FMT_FLT:
              nr_frames_read = (size_t) data_size / sizeof(float) /
                               (size_t) codec_context->channels;
              CHECK_FOR_PEAKS(data_float, -1.0f, 1.0f)
              result = ebur128_add_frames_float(st, data_float, nr_frames_read);
              CHECK_ERROR(result, "Internal EBU R128 error!\n", 1, close_codec)
              break;
            case SAMPLE_FMT_DBL:
              nr_frames_read = (size_t) data_size / sizeof(double) /
                               (size_t) codec_context->channels;
              CHECK_FOR_PEAKS(data_double, -1.0, 1.0)
              result = ebur128_add_frames_double(st, data_double, nr_frames_read);
              CHECK_ERROR(result, "Internal EBU R128 error!\n", 1, close_codec)
              break;
            case SAMPLE_FMT_NONE:
            case SAMPLE_FMT_NB:
            default:
              CHECK_ERROR(1, "Unknown sample format!\n", 1, close_codec)
              break;
          }
          packet.data += len;
          packet.size -= len;
        }
        packet.data = old_data;
      }
      av_free_packet(&packet);
    }

    segment_loudness[i - optind] = ebur128_gated_loudness_segment(st);
    if (ac != 2) {
      fprintf(stderr, "segment %d: %.2f LUFS\n", i + 1 - optind,
                      segment_loudness[i - optind]);
      ebur128_start_new_segment(st);
    }
    if (i == ac - 1) {
      gated_loudness = ebur128_gated_loudness_global(st);
      fprintf(stderr, "global loudness: %.2f LUFS\n", gated_loudness);
    }

  close_codec:
    avcodec_close(codec_context);

  close_file:
    av_close_input_file(format_context);
  }

  if (st && calculate_lra) {
    printf("LRA: %.2f\n", ebur128_loudness_range(st));
  }

  if (st && rgtag_exe) {
    char command[1024];
    double global_peak = 0.0;
    /* Get global peak */
    for (i = 0; i < ac - optind; ++i) {
      if (segment_peaks[i] > global_peak) {
        global_peak = segment_peaks[i];
      }
    }
    for (i = optind; i < ac; ++i) {
      if (segment_loudness[i - optind] < DBL_MAX &&
          gated_loudness < DBL_MAX) {
        snprintf(command, 1024, "%s \"%s\" %f %f %f %f", rgtag_exe, av[i],
                                -18.0 - segment_loudness[i - optind],
                                segment_peaks[i - optind],
                                -18.0 - gated_loudness,
                                global_peak);
        printf("%s\n", command);
        system(command);
      }
    }
  }

  if (st)
    ebur128_destroy(&st);
  if (segment_loudness)
    free(segment_loudness);

exit:
  return errcode;
}

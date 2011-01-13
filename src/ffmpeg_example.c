/* See LICENSE file for copyright and license details. */
#include <math.h>
#include <string.h>
#include <stdlib.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>

#include "./ebur128.h"

#define CHECK_ERROR(condition, message, errorcode, goto_point)                 \
  if ((condition)) {                                                           \
    fprintf(stderr, message);                                                  \
    errcode = (errorcode);                                                     \
    goto goto_point;                                                           \
  }


int main(int ac, const char* av[]) {
  AVFormatContext* format_context;
  AVCodecContext* codec_context;
  AVCodec* codec;
  AVPacket packet;

  ebur128_state* st = NULL;
  double gated_loudness;
  int errcode = 0;
  int result;

  CHECK_ERROR(ac < 2, "usage: r128-test FILENAME(S) ...\n", 1, exit)

  // Register all formats and codecs
  av_register_all();
  av_log_set_level(AV_LOG_ERROR);

  for (int i = 1; i < ac; ++i) {
    if (av_open_input_file(&format_context, av[i], NULL, 0, NULL) != 0) {
      continue;
    }
    if (av_find_stream_info(format_context) < 0) {
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
      goto close_file;
    }
    // Get a pointer to the codec context for the audio stream
    codec_context = format_context->streams[audio_stream]->codec;
    // Find the decoder for the video stream
    codec = avcodec_find_decoder(codec_context->codec_id);
    if (codec == NULL) {
      goto close_file;
    }
    // Open codec
    if (avcodec_open(codec_context, codec) < 0) {
      goto close_file;
    }


    if (!st) {
      st = ebur128_init(codec_context->channels,
                        codec_context->sample_rate,
                        EBUR128_MODE_M_I);
      CHECK_ERROR(!st, "Could not initialize EBU R128!\n", 1, close_codec)

      /* Special case seq-3341-6-5channels-16bit.wav.
       * Set channel map with function ebur128_set_channel_map. */
      if (codec_context->channels == 5) {
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
          int len = avcodec_decode_audio3(codec_context, (int16_t*) audio_buf, &data_size, &packet);
          if (len < 0) {
            packet.size = 0;
            break;
          }
          switch (codec_context->sample_fmt) {
            case SAMPLE_FMT_U8:
              CHECK_ERROR(1, "8 bit audio not supported by libebur128!\n", 1, close_codec)
              break;
            case SAMPLE_FMT_S16:
              result = ebur128_write_frames_short(st, data_short, (size_t) data_size / sizeof(int16_t) / (size_t) codec_context->channels);
              CHECK_ERROR(result, "Internal EBU R128 error!\n", 1, close_codec)
              break;
            case SAMPLE_FMT_S32:
              result = ebur128_write_frames_int(st, data_int, (size_t) data_size / sizeof(int32_t) / (size_t) codec_context->channels);
              CHECK_ERROR(result, "Internal EBU R128 error!\n", 1, close_codec)
              break;
            case SAMPLE_FMT_FLT:
              result = ebur128_write_frames_float(st, data_float, (size_t) data_size / sizeof(float) / (size_t) codec_context->channels);
              CHECK_ERROR(result, "Internal EBU R128 error!\n", 1, close_codec)
              break;
            case SAMPLE_FMT_DBL:
              result = ebur128_write_frames_double(st, data_double, (size_t) data_size / sizeof(double) / (size_t) codec_context->channels);
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

    if (ac != 2) {
      fprintf(stderr, "segment %d: %.1f LUFS\n", i,
                      ebur128_gated_loudness_segment(st));
      ebur128_start_new_segment(st);
    }
    if (i == ac - 1) {
      gated_loudness = ebur128_gated_loudness_global(st);
      fprintf(stderr, "global loudness: %.1f LUFS\n", gated_loudness);
    }

  close_codec:
    avcodec_close(codec_context);

  close_file:
    av_close_input_file(format_context);
  }

  if (st)
    ebur128_destroy(&st);

exit:
  return errcode;
}

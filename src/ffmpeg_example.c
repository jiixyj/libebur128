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
  AVFormatContext *pFormatCtx;
  AVCodecContext  *pCodecCtx;
  AVCodec         *pCodec;
  AVPacket        avpkt;

  ebur128_state* st = NULL;
  short* buffer;
  double gated_loudness;
  int errcode = 0;
  int result;

  CHECK_ERROR(ac < 2, "usage: r128-test FILENAME(S) ...\n", 1, exit)

  // Register all formats and codecs
  av_register_all();

  for (int i = 1; i < ac; ++i) {
    // Open audio file
    if (av_open_input_file(&pFormatCtx, av[i], NULL, 0, NULL) != 0) {
      return -1;
    }
    // Retrieve stream information
    if(av_find_stream_info(pFormatCtx) < 0) {
      return -1;
    }
    // Dump information about file onto standard error
    // dump_format(pFormatCtx, 0, av[1], 0);
    // Find the first audio stream
    int audioStream = -1;
    for (int j = 0; (unsigned) j < pFormatCtx->nb_streams; ++j) {
      if (pFormatCtx->streams[j]->codec->codec_type == CODEC_TYPE_AUDIO) {
        audioStream = j;
        break;
      }
    }
    if (audioStream == -1) {
      return -1;
    }
    // Get a pointer to the codec context for the video stream
    pCodecCtx = pFormatCtx->streams[audioStream]->codec;
    // Find the decoder for the video stream
    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    if (pCodec == NULL) {
      return -1;
    }
    // Open codec
    if (avcodec_open(pCodecCtx, pCodec) < 0) {
      return -1;
    }



    if (!st) {
      st = ebur128_init(pCodecCtx->channels,
                        pCodecCtx->sample_rate,
                        EBUR128_MODE_M_I);
      CHECK_ERROR(!st, "Could not initialize EBU R128!\n", 1, close_file)

      /* Special case seq-3341-6-5channels-16bit.wav.
       * Set channel map with function ebur128_set_channel_map. */
      if (pCodecCtx->channels == 5) {
        int channel_map_five[] = {EBUR128_LEFT,
                                  EBUR128_RIGHT,
                                  EBUR128_CENTER,
                                  EBUR128_LEFT_SURROUND,
                                  EBUR128_RIGHT_SURROUND};
        ebur128_set_channel_map(st, channel_map_five);
      }
    } else {
      CHECK_ERROR(st->channels != (size_t) pCodecCtx->channels ||
                  st->samplerate != (size_t) pCodecCtx->sample_rate,
                  "All files must have the same samplerate "
                  "and number of channels! Skipping...\n",
                  1, close_file)
    }

    buffer = (short*) malloc(st->samplerate * st->channels * sizeof(short));
    CHECK_ERROR(!buffer, "Could not allocate memory!\n", 1, close_file)
    while (av_read_frame(pFormatCtx, &avpkt) >= 0) {
      if (avpkt.stream_index == audioStream) {
        uint8_t audio_buf[AVCODEC_MAX_AUDIO_FRAME_SIZE];
        int16_t* data_short = (int16_t*) audio_buf;
        int32_t* data_int = (int32_t*) audio_buf;
        float* data_float = (float*) audio_buf;
        double* data_double = (double*) audio_buf;

        while (avpkt.size > 0) {
            int data_size = sizeof(audio_buf);
            int len = avcodec_decode_audio3(pCodecCtx, (int16_t*) audio_buf, &data_size, &avpkt);
            if (len < 0) {
                avpkt.size = 0;
                break;
            }
            // sample_fmt
            /* printf("total_samples=%d\n", total_samples);  */
            switch (pCodecCtx->sample_fmt) {
              case SAMPLE_FMT_U8:
                CHECK_ERROR(1, "8 bit audio not supported by libebur128!\n", 1, free_buffer)
                break;
              case SAMPLE_FMT_S16:
                result = ebur128_write_frames_short(st, data_short, (size_t) data_size / sizeof(int16_t) / (size_t) pCodecCtx->channels);
                CHECK_ERROR(result, "Internal EBU R128 error!\n", 1, free_buffer)
                break;
              case SAMPLE_FMT_S32:
                result = ebur128_write_frames_int(st, data_int, (size_t) data_size / sizeof(int32_t) / (size_t) pCodecCtx->channels);
                CHECK_ERROR(result, "Internal EBU R128 error!\n", 1, free_buffer)
                break;
              case SAMPLE_FMT_FLT:
                result = ebur128_write_frames_float(st, data_float, (size_t) data_size / sizeof(float) / (size_t) pCodecCtx->channels);
                CHECK_ERROR(result, "Internal EBU R128 error!\n", 1, free_buffer)
                break;
              case SAMPLE_FMT_DBL:
                result = ebur128_write_frames_double(st, data_double, (size_t) data_size / sizeof(double) / (size_t) pCodecCtx->channels);
                CHECK_ERROR(result, "Internal EBU R128 error!\n", 1, free_buffer)
                break;
              case SAMPLE_FMT_NONE:
              case SAMPLE_FMT_NB:
              default:
                CHECK_ERROR(1, "Unknown sample format!\n", 1, free_buffer)
                break;
            }
            avpkt.data += len;
            avpkt.size -= len;
        }
      }
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
  /*  if (sf_close(file)) {
      fprintf(stderr, "Could not close input file!\n");
    } */

  endloop: ;
  }

  if (st)
    ebur128_destroy(&st);

exit:
  return errcode;
}

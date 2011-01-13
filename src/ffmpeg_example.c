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

  size_t nr_frames_read;
  size_t nr_frames_read_all;
  ebur128_state* st = NULL;
  short* buffer;
  double gated_loudness;
  int errcode = 0;
  int result;
  int i;

  CHECK_ERROR(ac < 2, "usage: r128-test FILENAME(S) ...\n", 1, exit)

  // Register all formats and codecs
  av_register_all();

  for (i = 1; i < ac; ++i) {
    // Open audio file
    if (av_open_input_file(&pFormatCtx, av[i], NULL, 0, NULL) != 0) {
      return -1;
    }
    // Retrieve stream information
    if(av_find_stream_info(pFormatCtx) < 0) {
      return -1;
    }
    // Dump information about file onto standard error
    dump_format(pFormatCtx, 0, av[1], 0);
    // Find the first audio stream
    int audioStream = -1;
    for (int i = 0; (unsigned) i < pFormatCtx->nb_streams; ++i) {
      if (pFormatCtx->streams[i]->codec->codec_type == CODEC_TYPE_AUDIO) {
        audioStream = i;
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

    nr_frames_read_all = 0;


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
        int16_t *data = (int16_t *)audio_buf; 
        int data_size, len; 

        static int total_samples; 

        while (avpkt.size > 0) { 
            data_size = sizeof(audio_buf); 
            len = avcodec_decode_audio3(pCodecCtx, data, &data_size, &avpkt); 
            if (len < 0) { 
                avpkt.size = 0; 
                break;
            } 

            total_samples += data_size / 2 / pCodecCtx->channels; 
            /* printf("total_samples=%d\n", total_samples);  */
            result = ebur128_write_frames_short(st, data, (size_t) data_size / 2 / pCodecCtx->channels);
            CHECK_ERROR(result, "Internal EBU R128 error!\n", 1, free_buffer)



            avpkt.data += len; 
            avpkt.size -= len; 
        } 
      }
    }





    /*  nr_frames_read_all += nr_frames_read;
      result = ebur128_write_frames_short(st, buffer, (size_t) nr_frames_read);
      CHECK_ERROR(result, "Internal EBU R128 error!\n", 1, free_buffer)
    if (file_info.frames != nr_frames_read_all) {
      fprintf(stderr, "Warning: Could not read full file"
                              " or determine right length!\n");
    }*/

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

/* See LICENSE file for copyright and license details. */
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <glib.h>

#include "./ebur128.h"

#include "./common.h"

static GMutex* ffmpeg_mutex;

int init_input_library() {
  // Register all formats and codecs
  av_register_all();
  av_log_set_level(AV_LOG_ERROR);
  ffmpeg_mutex = g_mutex_new();
  return 0;
}

void exit_input_library() {
  g_mutex_free(ffmpeg_mutex);
  return;
}

void calculate_gain_of_file(void* user, void* user_data) {
  struct gain_data* gd = (struct gain_data*) user_data;
  size_t i = (size_t) user - 1;
  char* const* av = gd->file_names;
  double* segment_loudness = gd->segment_loudness;
  double* segment_peaks = gd->segment_peaks;
  int calculate_lra = gd->calculate_lra, tag_rg = gd->tag_rg;

  AVFormatContext* format_context;
  AVCodecContext* codec_context;
  AVCodec* codec;
  AVPacket packet;

  ebur128_state* st = NULL;

  int errcode, result;

  segment_loudness[i] = 0.0 / 0.0;

  g_mutex_lock(ffmpeg_mutex);

  if (av_open_input_file(&format_context, av[i], NULL, 0, NULL) != 0) {
    fprintf(stderr, "Could not open input file!\n");
    return;
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

  g_mutex_unlock(ffmpeg_mutex);

  st = ebur128_init(codec_context->channels,
                    codec_context->sample_rate,
                    EBUR128_MODE_I |
                    (calculate_lra ? EBUR128_MODE_LRA : 0));
  CHECK_ERROR(!st, "Could not initialize EBU R128!\n", 1, close_codec)
  gd->library_states[i] = st;

  if (codec_context->channel_layout) {
    int channel_map_index = 0;
    int bit_counter = 0;
    while (channel_map_index < codec_context->channels) {
      if (codec_context->channel_layout & (1 << bit_counter)) {
        switch (1 << bit_counter) {
          case CH_FRONT_LEFT:
            ebur128_set_channel(st, channel_map_index, EBUR128_LEFT);
            break;
          case CH_FRONT_RIGHT:
            ebur128_set_channel(st, channel_map_index, EBUR128_RIGHT);
            break;
          case CH_FRONT_CENTER:
            ebur128_set_channel(st, channel_map_index, EBUR128_CENTER);
            break;
          case CH_BACK_LEFT:
            ebur128_set_channel(st, channel_map_index, EBUR128_LEFT_SURROUND);
            break;
          case CH_BACK_RIGHT:
            ebur128_set_channel(st, channel_map_index, EBUR128_RIGHT_SURROUND);
            break;
          default:
            ebur128_set_channel(st, channel_map_index, EBUR128_UNUSED);
            break;
        }
        ++channel_map_index;
      }
      ++bit_counter;
    }
  } else if (codec_context->channels == 5) {
    /* Special case seq-3341-6-5channels-16bit.wav.
     * Set channel map with function ebur128_set_channel. */
    ebur128_set_channel(st, 0, EBUR128_LEFT);
    ebur128_set_channel(st, 1, EBUR128_RIGHT);
    ebur128_set_channel(st, 2, EBUR128_CENTER);
    ebur128_set_channel(st, 3, EBUR128_LEFT_SURROUND);
    ebur128_set_channel(st, 4, EBUR128_RIGHT_SURROUND);
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
        int len = avcodec_decode_audio3(codec_context, (int16_t*) audio_buf,
                                        &data_size, &packet);
#else
        int len = avcodec_decode_audio2(codec_context, (int16_t*) audio_buf,
                                        &data_size, packet.data, packet.size);
#endif
        if (len < 0) {
          packet.size = 0;
          break;
        }
    #define CHECK_FOR_PEAKS(buffer, min_scale, max_scale)                      \
        if (tag_rg) {                                                          \
          double scale_factor = -((double) min_scale) > (double) max_scale ?   \
                                -((double) min_scale) : (double) max_scale;    \
          size_t j;                                                            \
          double buffer_scaled;                                                \
          for (j = 0; j < (size_t) nr_frames_read * st->channels; ++j) {       \
            buffer_scaled = buffer[j] / (scale_factor);                        \
            if (buffer_scaled > segment_peaks[i])                              \
              segment_peaks[i] = buffer_scaled;                                \
            else if (-buffer_scaled > segment_peaks[i])                        \
              segment_peaks[i] = -buffer_scaled;                               \
          }                                                                    \
        }
        size_t nr_frames_read;
        switch (codec_context->sample_fmt) {
          case SAMPLE_FMT_U8:
            CHECK_ERROR(1, "8 bit audio not supported by libebur128!\n", 1,
                           close_codec)
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
            CHECK_FOR_PEAKS(data_int, INT_MIN, INT_MAX)
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
            result = ebur128_add_frames_double(st, data_double,
                                                   nr_frames_read);
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

  segment_loudness[i] = ebur128_loudness_global(st);
  fprintf(stderr, "*");

close_codec:
  g_mutex_lock(ffmpeg_mutex);
  avcodec_close(codec_context);
  g_mutex_unlock(ffmpeg_mutex);

close_file:
  g_mutex_lock(ffmpeg_mutex);
  av_close_input_file(format_context);
  g_mutex_unlock(ffmpeg_mutex);

  gd->errcode = errcode;
}

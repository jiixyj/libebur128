/* See LICENSE file for copyright and license details. */
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <glib.h>

#include "ebur128.h"

static GMutex* ffmpeg_mutex;

struct input_handle {
  AVFormatContext* format_context;
  AVCodecContext* codec_context;
  AVCodec* codec;
  AVPacket packet;
  int need_new_frame;
  int audio_stream;
  uint8_t* old_data;
  uint8_t audio_buf[AVCODEC_MAX_AUDIO_FRAME_SIZE + FF_INPUT_BUFFER_PADDING_SIZE];
  float buffer[(AVCODEC_MAX_AUDIO_FRAME_SIZE + FF_INPUT_BUFFER_PADDING_SIZE) / 2 + 1];
};

size_t input_get_channels(struct input_handle* ih) {
  return (size_t) ih->codec_context->channels;
}

size_t input_get_samplerate(struct input_handle* ih) {
  return (size_t) ih->codec_context->sample_rate;
}

float* input_get_buffer(struct input_handle* ih) {
  return ih->buffer;
}

size_t input_get_buffer_size(struct input_handle* ih) {
  (void) ih;
  return (AVCODEC_MAX_AUDIO_FRAME_SIZE + FF_INPUT_BUFFER_PADDING_SIZE) / 2 + 1;
}

struct input_handle* input_handle_init() {
  struct input_handle* ret;
  ret = malloc(sizeof(struct input_handle));
  return ret;
}

void input_handle_destroy(struct input_handle** ih) {
  free(*ih);
  *ih = NULL;
}


int input_open_file(struct input_handle* ih, const char* filename) {
  g_mutex_lock(ffmpeg_mutex);
  if (av_open_input_file(&ih->format_context, filename, NULL, 0, NULL) != 0) {
    fprintf(stderr, "Could not open input file!\n");
    g_mutex_unlock(ffmpeg_mutex);
    return 1;
  }
  if (av_find_stream_info(ih->format_context) < 0) {
    fprintf(stderr, "Could not find stream info!\n");
    g_mutex_unlock(ffmpeg_mutex);
    goto close_file;
  }
  // Dump information about file onto standard error
  // dump_format(format_context, 0, av[1], 0);

  // Find the first audio stream
  ih->audio_stream = -1;
  for (size_t j = 0; j < ih->format_context->nb_streams; ++j) {
    if (ih->format_context->streams[j]->codec->codec_type == CODEC_TYPE_AUDIO) {
      ih->audio_stream = (int) j;
      break;
    }
  }
  if (ih->audio_stream == -1) {
    fprintf(stderr, "Could not find an audio stream in file!\n");
    g_mutex_unlock(ffmpeg_mutex);
    goto close_file;
  }
  // Get a pointer to the codec context for the audio stream
  ih->codec_context = ih->format_context->streams[ih->audio_stream]->codec;
  // Find the decoder for the video stream
  ih->codec = avcodec_find_decoder(ih->codec_context->codec_id);
  if (ih->codec == NULL) {
    fprintf(stderr, "Could not find a decoder for the audio format!\n");
    g_mutex_unlock(ffmpeg_mutex);
    goto close_file;
  }
  // Open codec
  if (avcodec_open(ih->codec_context, ih->codec) < 0) {
    fprintf(stderr, "Could not open the codec!\n");
    g_mutex_unlock(ffmpeg_mutex);
    goto close_file;
  }
  g_mutex_unlock(ffmpeg_mutex);
  ih->need_new_frame = TRUE;
  ih->old_data = NULL;
  return 0;

close_file:
  g_mutex_lock(ffmpeg_mutex);
  av_close_input_file(ih->format_context);
  g_mutex_unlock(ffmpeg_mutex);
  return 1;
}

int input_set_channel_map(struct input_handle* ih, ebur128_state* st) {
  if (ih->codec_context->channel_layout) {
    size_t channel_map_index = 0;
    int bit_counter = 0;
    while (channel_map_index < (size_t) ih->codec_context->channels) {
      if (ih->codec_context->channel_layout & (1 << bit_counter)) {
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
    return 0;
  } else {
    return 1;
  }
}

int input_allocate_buffer(struct input_handle* ih) {
  (void) ih;
  return 0;
}

size_t input_read_frames(struct input_handle* ih) {
  for (;;) {
    if (ih->need_new_frame && av_read_frame(ih->format_context, &ih->packet) < 0) {
      return 0;
    }
    ih->need_new_frame = FALSE;
    if (ih->packet.stream_index == ih->audio_stream) {
      int16_t* data_short =  (int16_t*) ih->audio_buf;
      int32_t* data_int =    (int32_t*) ih->audio_buf;
      float*   data_float =  (float*)   ih->audio_buf;
      double*  data_double = (double*)  ih->audio_buf;

      if (!ih->old_data) {
        ih->old_data = ih->packet.data;
      }
      while (ih->packet.size > 0) {
        int data_size = sizeof(ih->audio_buf);
#if LIBAVCODEC_VERSION_MAJOR >= 52 &&  \
    LIBAVCODEC_VERSION_MINOR >= 26 &&  \
    LIBAVCODEC_VERSION_MICRO >= 0
        int len = avcodec_decode_audio3(ih->codec_context, (int16_t*) ih->audio_buf,
                                        &data_size, &ih->packet);
#else
        int len = avcodec_decode_audio2(ih->codec_context, (int16_t*) ih->audio_buf,
                                        &data_size, ih->packet.data, ih->packet.size);
#endif
        if (len < 0) {
          ih->packet.size = 0;
          break;
        }
        ih->packet.data += len;
        ih->packet.size -= len;
        if (!data_size) {
          continue;
        }
        size_t nr_frames_read;
        switch (ih->codec_context->sample_fmt) {
          case SAMPLE_FMT_U8:
            fprintf(stderr, "8 bit audio not supported by libebur128!\n");
            return 0;
            break;
          case SAMPLE_FMT_S16:
            nr_frames_read = (size_t) data_size / sizeof(int16_t) /
                             (size_t) ih->codec_context->channels;
            for (size_t i = 0; i < (size_t) data_size / sizeof(int16_t); ++i) {
              ih->buffer[i] = ((float) data_short[i]) /
                              MAX(-(float) SHRT_MIN, (float) SHRT_MAX);
            }
            break;
          case SAMPLE_FMT_S32:
            nr_frames_read = (size_t) data_size / sizeof(int32_t) /
                             (size_t) ih->codec_context->channels;
            for (size_t i = 0; i < (size_t) data_size / sizeof(int32_t); ++i) {
              ih->buffer[i] = ((float) data_int[i]) /
                              MAX(-(float) INT_MIN, (float) INT_MAX);
            }
            break;
          case SAMPLE_FMT_FLT:
            nr_frames_read = (size_t) data_size / sizeof(float) /
                             (size_t) ih->codec_context->channels;
            for (size_t i = 0; i < (size_t) data_size / sizeof(float); ++i) {
              ih->buffer[i] = data_float[i];
            }
            break;
          case SAMPLE_FMT_DBL:
            nr_frames_read = (size_t) data_size / sizeof(double) /
                             (size_t) ih->codec_context->channels;
            for (size_t i = 0; i < (size_t) data_size / sizeof(double); ++i) {
              ih->buffer[i] = (float) data_double[i];
            }
            break;
          case SAMPLE_FMT_NONE:
          case SAMPLE_FMT_NB:
          default:
            fprintf(stderr, "Unknown sample format!\n");
            return 0;
            break;
        }
        return nr_frames_read;
      }
      ih->packet.data = ih->old_data;
      ih->old_data = NULL;
    }
    av_free_packet(&ih->packet);
    ih->need_new_frame = TRUE;
  }
}

int input_check_ok(struct input_handle* ih, size_t nr_frames_read_all) {
  (void) ih;
  (void) nr_frames_read_all;
  return 0;
}

void input_free_buffer(struct input_handle* ih) {
  (void) ih;
  return;
}

void input_close_file(struct input_handle* ih) {
  g_mutex_lock(ffmpeg_mutex);
  avcodec_close(ih->codec_context);
  av_close_input_file(ih->format_context);
  g_mutex_unlock(ffmpeg_mutex);
}

int input_init_library() {
  // Register all formats and codecs
  av_register_all();
  av_log_set_level(AV_LOG_ERROR);
  ffmpeg_mutex = g_mutex_new();
  return 0;
}

void input_exit_library() {
  g_mutex_free(ffmpeg_mutex);
  return;
}

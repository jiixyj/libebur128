/* See LICENSE file for copyright and license details. */
#include <libavformat/avformat.h>
#include <gmodule.h>

#include "ebur128.h"
#include "input.h"

static GStaticMutex ffmpeg_mutex = G_STATIC_MUTEX_INIT;

#define BUFFER_SIZE (AVCODEC_MAX_AUDIO_FRAME_SIZE + FF_INPUT_BUFFER_PADDING_SIZE)

struct _buffer {
  uint8_t audio_buf[BUFFER_SIZE];
} __attribute__ ((aligned (16)));

struct input_handle {
  AVFormatContext* format_context;
  AVCodecContext* codec_context;
  AVCodec* codec;
  AVPacket packet;
  int need_new_frame;
  int audio_stream;
  uint8_t* old_data;
  struct _buffer audio_buf;
  float buffer[BUFFER_SIZE / 2 + 1];
};

static unsigned ffmpeg_get_channels(struct input_handle* ih) {
  return (unsigned) ih->codec_context->channels;
}

static unsigned long ffmpeg_get_samplerate(struct input_handle* ih) {
  return (unsigned long) ih->codec_context->sample_rate;
}

static float* ffmpeg_get_buffer(struct input_handle* ih) {
  return ih->buffer;
}

static struct input_handle* ffmpeg_handle_init() {
  struct input_handle* ret;
  ret = malloc(sizeof(struct input_handle));

  av_init_packet(&ret->packet);

  return ret;
}

static void ffmpeg_handle_destroy(struct input_handle** ih) {
  free(*ih);
  *ih = NULL;
}


static int ffmpeg_open_file(struct input_handle* ih, const char* filename) {
  size_t j;

  g_static_mutex_lock(&ffmpeg_mutex);
  ih->format_context = NULL;

  if (avformat_open_input(&ih->format_context, filename, NULL, NULL) != 0) {
    fprintf(stderr, "Could not open input file!\n");
    g_static_mutex_unlock(&ffmpeg_mutex);
    return 1;
  }
  if (av_find_stream_info(ih->format_context) < 0) {
    fprintf(stderr, "Could not find stream info!\n");
    g_static_mutex_unlock(&ffmpeg_mutex);
    goto close_file;
  }
  // av_dump_format(ih->format_context, 0, "blub", 0);

  // Find the first audio stream
  ih->audio_stream = -1;
  for (j = 0; j < ih->format_context->nb_streams; ++j) {
    if (ih->format_context->streams[j]->codec->codec_type
            == AVMEDIA_TYPE_AUDIO) {
      ih->audio_stream = (int) j;
      break;
    }
  }
  if (ih->audio_stream == -1) {
    fprintf(stderr, "Could not find an audio stream in file!\n");
    g_static_mutex_unlock(&ffmpeg_mutex);
    goto close_file;
  }
  // Get a pointer to the codec context for the audio stream
  ih->codec_context = ih->format_context->streams[ih->audio_stream]->codec;

  ih->codec_context->request_sample_fmt = AV_SAMPLE_FMT_FLT;

  ih->codec = avcodec_find_decoder(ih->codec_context->codec_id);
  if (ih->codec == NULL) {
    fprintf(stderr, "Could not find a decoder for the audio format!\n");
    g_static_mutex_unlock(&ffmpeg_mutex);
    goto close_file;
  }

  char *float_codec = g_malloc(strlen(ih->codec->name) + sizeof("float") + 1);
  sprintf(float_codec, "%sfloat", ih->codec->name);
  AVCodec *possible_float_codec = avcodec_find_decoder_by_name(float_codec);
  if (possible_float_codec)
    ih->codec = possible_float_codec;
  g_free(float_codec);

  // Open codec
  if (avcodec_open(ih->codec_context, ih->codec) < 0) {
    fprintf(stderr, "Could not open the codec!\n");
    g_static_mutex_unlock(&ffmpeg_mutex);
    goto close_file;
  }
  g_static_mutex_unlock(&ffmpeg_mutex);
  ih->need_new_frame = TRUE;
  ih->old_data = NULL;

  return 0;

close_file:
  g_static_mutex_lock(&ffmpeg_mutex);
  av_close_input_file(ih->format_context);
  g_static_mutex_unlock(&ffmpeg_mutex);
  return 1;
}

static int ffmpeg_set_channel_map(struct input_handle* ih, int* st) {
  if (ih->codec_context->channel_layout) {
    unsigned int channel_map_index = 0;
    int bit_counter = 0;
    while (channel_map_index < (unsigned) ih->codec_context->channels) {
      if (ih->codec_context->channel_layout & (1 << bit_counter)) {
        switch (1 << bit_counter) {
          case AV_CH_FRONT_LEFT:
            st[channel_map_index] = EBUR128_LEFT;
            break;
          case AV_CH_FRONT_RIGHT:
            st[channel_map_index] = EBUR128_RIGHT;
            break;
          case AV_CH_FRONT_CENTER:
            st[channel_map_index] = EBUR128_CENTER;
            break;
          case AV_CH_BACK_LEFT:
            st[channel_map_index] = EBUR128_LEFT_SURROUND;
            break;
          case AV_CH_BACK_RIGHT:
            st[channel_map_index] = EBUR128_RIGHT_SURROUND;
            break;
          default:
            st[channel_map_index] = EBUR128_UNUSED;
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

static int ffmpeg_allocate_buffer(struct input_handle* ih) {
  (void) ih;
  return 0;
}

static size_t ffmpeg_get_total_frames(struct input_handle* ih) {
  double tmp = (double) ih->format_context->streams[ih->audio_stream]->duration
             * (double) ih->format_context->streams[ih->audio_stream]->time_base.num
             / (double) ih->format_context->streams[ih->audio_stream]->time_base.den
             * (double) ih->codec_context->sample_rate;

  if (tmp <= 0.0) {
    return 0;
  } else {
    return (size_t) (tmp + 0.5);
  }
}

static size_t ffmpeg_read_one_packet(struct input_handle* ih) {
next_frame:
  do {
    if (av_read_frame(ih->format_context, &ih->packet) < 0) {
      return 0;
    }
  } while (ih->packet.stream_index != ih->audio_stream);

  size_t nr_frames_read, i;

  int data_size = sizeof(ih->audio_buf);
  int len = avcodec_decode_audio3(ih->codec_context,
                                  (int16_t*) &ih->audio_buf,
                                  &data_size, &ih->packet);
  if (len < 0 || data_size < 0) {
    fprintf(stderr, "Error in decoder!\n");
    nr_frames_read = 0;
    goto out;
  }

  /* No data used, (happens with metadata frames for example) */
  if (len <= 0 || data_size <= 0) {
    av_free_packet(&ih->packet);
    goto next_frame;
  }

  int16_t* data_short =  (int16_t*) &ih->audio_buf;
  int32_t* data_int =    (int32_t*) &ih->audio_buf;
  float*   data_float =  (float*)   &ih->audio_buf;
  double*  data_double = (double*)  &ih->audio_buf;

  switch (ih->codec_context->sample_fmt) {
    case AV_SAMPLE_FMT_U8:
      fprintf(stderr, "8 bit audio not supported by libebur128!\n");
      nr_frames_read = 0;
      goto out;
    case AV_SAMPLE_FMT_S16:
      nr_frames_read = (size_t) data_size / sizeof(int16_t) /
                       (size_t) ih->codec_context->channels;
      for (i = 0; i < (size_t) data_size / sizeof(int16_t); ++i) {
        ih->buffer[i] = ((float) data_short[i]) /
                        MAX(-(float) SHRT_MIN, (float) SHRT_MAX);
      }
      break;
    case AV_SAMPLE_FMT_S32:
      nr_frames_read = (size_t) data_size / sizeof(int32_t) /
                       (size_t) ih->codec_context->channels;
      for (i = 0; i < (size_t) data_size / sizeof(int32_t); ++i) {
        ih->buffer[i] = ((float) data_int[i]) /
                        MAX(-(float) INT_MIN, (float) INT_MAX);
      }
      break;
    case AV_SAMPLE_FMT_FLT:
      nr_frames_read = (size_t) data_size / sizeof(float) /
                       (size_t) ih->codec_context->channels;
      for (i = 0; i < (size_t) data_size / sizeof(float); ++i) {
        ih->buffer[i] = data_float[i];
      }
      break;
    case AV_SAMPLE_FMT_DBL:
      nr_frames_read = (size_t) data_size / sizeof(double) /
                       (size_t) ih->codec_context->channels;
      for (i = 0; i < (size_t) data_size / sizeof(double); ++i) {
        ih->buffer[i] = (float) data_double[i];
      }
      break;
    case AV_SAMPLE_FMT_NONE:
    case AV_SAMPLE_FMT_NB:
    default:
      fprintf(stderr, "Unknown sample format!\n");
      nr_frames_read = 0;
      goto out;
  }
out:
  av_free_packet(&ih->packet);
  return nr_frames_read;
}

static size_t ffmpeg_read_frames(struct input_handle* ih) {
  return ffmpeg_read_one_packet(ih);
}

static void ffmpeg_free_buffer(struct input_handle* ih) {
  (void) ih;
  return;
}

static void ffmpeg_close_file(struct input_handle* ih) {
  g_static_mutex_lock(&ffmpeg_mutex);
  avcodec_close(ih->codec_context);
  av_close_input_file(ih->format_context);
  g_static_mutex_unlock(&ffmpeg_mutex);
}

static int ffmpeg_init_library() {
  // Register all formats and codecs
  av_register_all();
  av_log_set_level(AV_LOG_ERROR);
  return 0;
}

static void ffmpeg_exit_library() {
  return;
}

G_MODULE_EXPORT struct input_ops ip_ops = {
  ffmpeg_get_channels,
  ffmpeg_get_samplerate,
  ffmpeg_get_buffer,
  ffmpeg_handle_init,
  ffmpeg_handle_destroy,
  ffmpeg_open_file,
  ffmpeg_set_channel_map,
  ffmpeg_allocate_buffer,
  ffmpeg_get_total_frames,
  ffmpeg_read_frames,
  ffmpeg_free_buffer,
  ffmpeg_close_file,
  ffmpeg_init_library,
  ffmpeg_exit_library
};

G_MODULE_EXPORT const char* ip_exts[] = {"wav", "flac", "ogg", "oga", "mp3", "mp2", "mpc", "ac3", "wv", "mpg", "avi", "mkv", "m4a", "mp4", "aac", "mov", "mxf", NULL};

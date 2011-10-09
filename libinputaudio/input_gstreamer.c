/* See LICENSE file for copyright and license details. */
#include <gst/gst.h>
#include <gst/app/gstappsink.h>

#include "input.h"

struct input_handle {
  const char *filename;
  GThread *gstreamer_loop;
  gboolean quit_pipeline;
  gboolean ready;
  gboolean main_loop_quit;

  GMainContext *main_context;
  GSource *message_source;

  GstElement *bin;
  GstElement *appsink;
  GMainLoop *loop;

  GSList *buffer_list;
  size_t current_bytes;

  float *buffer;
};

static GStaticMutex gstreamer_mutex = G_STATIC_MUTEX_INIT;
extern gboolean verbose;

static gboolean bus_call(GstBus *bus, GstMessage *msg, gpointer data)
{
  struct input_handle *ih = (struct input_handle *) data;

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ASYNC_DONE:{
      ih->quit_pipeline = FALSE;
      ih->ready = TRUE;
      break;
    }
    case GST_MESSAGE_EOS:{
      if (verbose) g_print("End-of-stream\n");
      ih->ready = TRUE;
      g_static_mutex_lock(&gstreamer_mutex);
      if (!ih->main_loop_quit) {
        ih->main_loop_quit = TRUE;
        g_main_loop_quit(ih->loop);
      }
      g_static_mutex_unlock(&gstreamer_mutex);
      break;
    }
    case GST_MESSAGE_ERROR:{
      gchar *debug;
      GError *err;

      gst_message_parse_error (msg, &err, &debug);
      g_free (debug);

      if (verbose) g_print("%p Error: %s\n", bus, err->message);
      g_error_free (err);

      ih->ready = TRUE;
      g_static_mutex_lock(&gstreamer_mutex);
      if (!ih->main_loop_quit) {
        ih->main_loop_quit = TRUE;
        g_main_loop_quit(ih->loop);
      }
      g_static_mutex_unlock(&gstreamer_mutex);
      break;
    }
    case GST_MESSAGE_STATE_CHANGED: {
      GstState old_state, new_state;

      gst_message_parse_state_changed (msg, &old_state, &new_state, NULL);
      /* if (verbose) g_print ("Element %s changed state from %s to %s.\n",
             GST_OBJECT_NAME (msg->src),
             gst_element_state_get_name (old_state),
             gst_element_state_get_name (new_state)); */
      break;
    }
    case GST_MESSAGE_STREAM_STATUS:{
      GstStreamStatusType type;
      GstElement *owner;
      gst_message_parse_stream_status(msg, &type, &owner);
      /* if (verbose) g_print("%p New Stream Type: %d\n", bus, type); */
      break;
    }
    default:
      break;
  }
  return TRUE;
}

static gint query_data(struct input_handle* ih, gboolean query_rate) {
  GstElement *converter;
  GstPad *src_pad;
  GstCaps *src_caps;
  GstStructure *s;
  gint rate, channels;

  converter = gst_bin_get_by_name(GST_BIN(ih->bin), "converter");
  src_pad = gst_element_get_static_pad(converter, "src");
  src_caps = gst_pad_get_caps(src_pad);

  s = gst_caps_get_structure(src_caps, 0);
  gst_structure_get_int(s, "rate", &rate);
  gst_structure_get_int(s, "channels", &channels);
  /* g_print ("%d channels @ %d Hz\n", channels, rate); */

  g_object_unref(src_pad);
  g_object_unref(converter);
  return query_rate ? rate : channels;
}

static unsigned gstreamer_get_channels(struct input_handle* ih) {
  return (unsigned) query_data(ih, FALSE);
}

static unsigned long gstreamer_get_samplerate(struct input_handle* ih) {
  return (unsigned long) query_data(ih, TRUE);
}

static float* gstreamer_get_buffer(struct input_handle* ih) {
  return ih->buffer;
}

static struct input_handle* gstreamer_handle_init() {
  struct input_handle* ret;
  ret = malloc(sizeof(struct input_handle));

  ret->buffer_list = NULL;
  ret->current_bytes = 0;

  ret->main_context = g_main_context_new();

  return ret;
}

static gpointer gstreamer_loop(struct input_handle *ih) {
  GstElement *fdsrc;
  GError *error = NULL;
  GstBus *bus;

  ih->bin = gst_parse_launch("filesrc name=my_fdsrc ! "
                             "decodebin2 ! "
                             "audioconvert name=converter ! "
                             "audio/x-raw-float,width=32,endianness=1234 ! "
                             "appsink name=sink sync=FALSE", &error);
  if (!ih->bin) {
    fprintf(stderr, "Parse error: %s", error->message);
    return NULL;
  }

  fdsrc = gst_bin_get_by_name(GST_BIN(ih->bin), "my_fdsrc");
  g_object_set(G_OBJECT(fdsrc), "location", ih->filename, NULL);

  ih->appsink = gst_bin_get_by_name(GST_BIN(ih->bin), "sink");
  /* gst_app_sink_set_max_buffers(GST_APP_SINK(ih->appsink), 1); */

  /* start playing */
  ih->loop = g_main_loop_new(ih->main_context, FALSE);

  bus = gst_element_get_bus(ih->bin);
  ih->message_source = gst_bus_create_watch(bus);
  g_source_set_callback(ih->message_source, (GSourceFunc) bus_call, ih, NULL);
  g_source_attach(ih->message_source, ih->main_context);
  g_source_unref(ih->message_source);
  g_object_unref(bus);

  /* start play back and listed to events */
  gst_element_set_state(ih->bin, GST_STATE_PLAYING);

  g_main_loop_run(ih->loop);

  return NULL;
}

static int gstreamer_open_file(struct input_handle* ih, const char* filename) {
  GTimeVal beg, end;

  ih->filename = filename;
  ih->quit_pipeline = TRUE;
  ih->main_loop_quit = FALSE;
  ih->ready = FALSE;
  ih->bin = NULL;
  ih->message_source = NULL;
  ih->gstreamer_loop = g_thread_create((GThreadFunc) gstreamer_loop, ih, TRUE, NULL);

  g_get_current_time(&beg);
  while (!ih->ready) {
    g_thread_yield();
    g_get_current_time(&end);
    if (end.tv_usec + end.tv_sec * G_USEC_PER_SEC
      - beg.tv_usec - beg.tv_sec * G_USEC_PER_SEC > 1 * G_USEC_PER_SEC) {
      break;
    }
  }

  if (ih->quit_pipeline) {
    if (ih->bin) {
      GstBus *bus = gst_element_get_bus(ih->bin);
      gst_bus_post(bus, gst_message_new_eos(NULL));
      g_object_unref(bus);
    }
    g_thread_join(ih->gstreamer_loop);
    if (ih->message_source) g_source_destroy(ih->message_source);
    if (ih->bin) {
      /* cleanup */
      gst_element_set_state(ih->bin, GST_STATE_NULL);
      g_object_unref(ih->bin);
      ih->bin = NULL;
      g_main_loop_unref(ih->loop);
    }
    return 1;
  } else {
    return 0;
  }
}

static int gstreamer_set_channel_map(struct input_handle* ih, ebur128_state* st) {
  (void) ih;
  (void) st;
  return 1;
}

static void gstreamer_handle_destroy(struct input_handle** ih) {
  g_main_context_unref((*ih)->main_context);
  free(*ih);
  *ih = NULL;
}

#define BUFFER_SIZE (44100 * sizeof(float))
static int gstreamer_allocate_buffer(struct input_handle* ih) {
  ih->buffer = g_malloc(BUFFER_SIZE);
  return 0;
}

static size_t gstreamer_get_total_frames(struct input_handle* ih) {
  gint64 time = 0;
  GstFormat format = GST_FORMAT_TIME;

  if (gst_element_query_duration(ih->bin, &format, &time)) {
      double tmp = time * 1e-9 * 44100;
      if (tmp <= 0.0) {
          return 0;
      } else {
          return (size_t) (tmp + 0.5);
      }
  } else {
      return 0;
  }
}

static size_t gstreamer_read_frames(struct input_handle* ih) {
    size_t buf_pos = 0;
    GSList *next;

    while (ih->current_bytes < BUFFER_SIZE) {
        GstBuffer *buf = gst_app_sink_pull_buffer(GST_APP_SINK(ih->appsink));
        if (!buf) {
            break;
        }
        ih->buffer_list = g_slist_append(ih->buffer_list, buf);
        ih->current_bytes += buf->size;
    }

    while (ih->buffer_list &&
           GST_BUFFER(ih->buffer_list->data)->size + buf_pos <= BUFFER_SIZE) {
        memcpy((guint8 *) ih->buffer + buf_pos,
               GST_BUFFER(ih->buffer_list->data)->data,
               GST_BUFFER(ih->buffer_list->data)->size);
        buf_pos           += GST_BUFFER(ih->buffer_list->data)->size;
        ih->current_bytes -= GST_BUFFER(ih->buffer_list->data)->size;

        gst_buffer_unref(GST_BUFFER(ih->buffer_list->data));
        next = ih->buffer_list->next;
        g_slist_free_1(ih->buffer_list);
        ih->buffer_list = next;
    }
    return buf_pos / sizeof(float) / gstreamer_get_channels(ih);
}

static void gstreamer_free_buffer(struct input_handle* ih) {
  g_free(ih->buffer);
  return;
}

static void gstreamer_close_file(struct input_handle* ih) {
  if (ih->bin) {
    GstBus *bus = gst_element_get_bus(ih->bin);
    gst_bus_post(bus, gst_message_new_eos(NULL));
    g_object_unref(bus);
  }
  g_thread_join(ih->gstreamer_loop);
  if (ih->message_source) g_source_destroy(ih->message_source);
  if (ih->bin) {
    /* cleanup */
    gst_element_set_state(ih->bin, GST_STATE_NULL);
    g_object_unref(ih->bin);
    ih->bin = NULL;
    g_main_loop_unref(ih->loop);
  }
}

static int gstreamer_init_library() {
  gst_init(NULL, NULL);
  return 0;
}

static void gstreamer_exit_library() {
  gst_deinit();
  return;
}

#ifdef GSTREAMER_INPUT_STATIC
struct input_ops gstreamer_ip_ops =
#else
G_MODULE_EXPORT struct input_ops ip_ops =
#endif
{
  gstreamer_get_channels,
  gstreamer_get_samplerate,
  gstreamer_get_buffer,
  gstreamer_handle_init,
  gstreamer_handle_destroy,
  gstreamer_open_file,
  gstreamer_set_channel_map,
  gstreamer_allocate_buffer,
  gstreamer_get_total_frames,
  gstreamer_read_frames,
  gstreamer_free_buffer,
  gstreamer_close_file,
  gstreamer_init_library,
  gstreamer_exit_library
};

#ifdef GSTREAMER_INPUT_STATIC
const char *gstreamer_ip_exts[] =
#else
G_MODULE_EXPORT const char *ip_exts[] =
#endif
{ NULL };

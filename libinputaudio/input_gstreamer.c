/* See LICENSE file for copyright and license details. */
#include <gst/gst.h>
#include <gst/app/gstappsink.h>

#include "input.h"

struct input_handle {
  GstElement *bin;
  GstElement *appsink;
  GThread *event_thread;

  GSList *buffer_list;
  size_t current_bytes;

  float *buffer;
};

static GStaticMutex gstreamer_mutex = G_STATIC_MUTEX_INIT;

static gpointer event_loop(gpointer user)
{
  GstBus *bus;
  GstMessage *message = NULL;
  gboolean running = TRUE;
  struct input_handle *ih = (struct input_handle *) user;

  bus = gst_element_get_bus (GST_ELEMENT (ih->bin));

  while (running) {
    g_static_mutex_lock(&gstreamer_mutex);
    message = gst_bus_poll(bus, GST_MESSAGE_ANY, -1);
    g_static_mutex_unlock(&gstreamer_mutex);
    g_assert (message != NULL);
    switch (message->type) {
      case GST_MESSAGE_EOS:
        running = FALSE;
        break;
      case GST_MESSAGE_WARNING:{
        GError *gerror;
        gchar *debug;

        gst_message_parse_warning (message, &gerror, &debug);
        gst_object_default_error (GST_MESSAGE_SRC (message), gerror, debug);
        g_error_free (gerror);
        g_free (debug);
        break;
      }
      case GST_MESSAGE_ERROR:{
        GError *gerror;
        gchar *debug;

        gst_message_parse_error (message, &gerror, &debug);
        gst_object_default_error (GST_MESSAGE_SRC (message), gerror, debug);
        g_error_free (gerror);
        g_free (debug);
        running = FALSE;
        gst_element_set_state(ih->bin, GST_STATE_NULL);
        break;
      }
      default:
        break;
    }
    gst_message_unref (message);
  }
  gst_object_unref (bus);

  return NULL;
}

static unsigned gstreamer_get_channels(struct input_handle* ih) {
    /* FIXME */
  return 2;
}

static unsigned long gstreamer_get_samplerate(struct input_handle* ih) {
    /* FIXME */
  return 44100;
}

static float* gstreamer_get_buffer(struct input_handle* ih) {
  return ih->buffer;
}

static size_t gstreamer_get_buffer_size(struct input_handle* ih) {
    /* FIXME */
  return 0;
}

static struct input_handle* gstreamer_handle_init() {
  struct input_handle* ret;
  ret = malloc(sizeof(struct input_handle));

  ret->buffer_list = NULL;
  ret->current_bytes = 0;

  return ret;
}

static int gstreamer_open_file(struct input_handle* ih, FILE* file, const char* filename) {
  GstElement *fdsrc;
  GError *error = NULL;
  GstBuffer *preroll = NULL;

  ih->bin = gst_parse_launch("filesrc name=my_fdsrc ! "
                             "decodebin2 ! "
                             "audioconvert ! "
                             "audio/x-raw-float,width=32,endianness=1234 ! "
                             "appsink name=sink sync=FALSE", &error);
  if (!ih->bin) {
    fprintf(stderr, "Parse error: %s", error->message);
    return 1;
  }

  fdsrc = gst_bin_get_by_name(GST_BIN(ih->bin), "my_fdsrc");
  g_object_set(G_OBJECT(fdsrc), "location", filename, NULL);

  ih->appsink = gst_bin_get_by_name(GST_BIN(ih->bin), "sink");
  // gst_app_sink_set_max_buffers(GST_APP_SINK(ih->appsink), 1);

  /* start playing */
  gst_element_set_state(ih->bin, GST_STATE_PLAYING);
  ih->event_thread = g_thread_create(event_loop, ih, TRUE, NULL);

  g_signal_emit_by_name(ih->appsink, "pull-preroll", &preroll);
  if (!preroll) {
    gst_element_set_state(ih->bin, GST_STATE_NULL);
    g_thread_join(ih->event_thread);
    return 1;
  } else {
    gst_buffer_unref(preroll);
    return 0;
  }
}

static int gstreamer_set_channel_map(struct input_handle* ih, ebur128_state* st) {
  (void) ih;
  (void) st;
  return 1;
}

static void gstreamer_handle_destroy(struct input_handle** ih) {
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
    // if (ih->gst_buf->size > BUFFER_SIZE) {
    //     fprintf(stderr, "Buffer too small for %lu bytes!\n", ih->gst_buf->size);
    //     return 0;
    // }
    // memcpy(ih->buffer, ih->gst_buf->data, ih->gst_buf->size);
    return buf_pos / sizeof(float) / gstreamer_get_channels(ih);
}

static int gstreamer_check_ok(struct input_handle* ih, size_t nr_frames_read_all) {
  if (gstreamer_get_total_frames(ih) != nr_frames_read_all) {
    return 1;
  } else {
    return 0;
  }
}

static void gstreamer_free_buffer(struct input_handle* ih) {
  g_free(ih->buffer);
  return;
}

static void gstreamer_close_file(struct input_handle* ih, FILE* file) {
  (void) file;
  gst_element_set_state(ih->bin, GST_STATE_NULL);
  gst_object_unref(ih->bin);
}

static int gstreamer_init_library() {
  gst_init(NULL, NULL);
  return 0;
}

static void gstreamer_exit_library() {
  gst_deinit();
  return;
}

G_MODULE_EXPORT struct input_ops ip_ops = {
  gstreamer_get_channels,
  gstreamer_get_samplerate,
  gstreamer_get_buffer,
  gstreamer_get_buffer_size,
  gstreamer_handle_init,
  gstreamer_handle_destroy,
  gstreamer_open_file,
  gstreamer_set_channel_map,
  gstreamer_allocate_buffer,
  gstreamer_get_total_frames,
  gstreamer_read_frames,
  gstreamer_check_ok,
  gstreamer_free_buffer,
  gstreamer_close_file,
  gstreamer_init_library,
  gstreamer_exit_library
};

G_MODULE_EXPORT const char* ip_exts[] = { NULL };

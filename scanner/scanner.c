/* See LICENSE file for copyright and license details. */
#define _POSIX_C_SOURCE 200112L         /* needed for isnan() and isinf() */
#include <float.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <glib.h>
#include <glib/gstdio.h>

#include "ebur128.h"
#include "input.h"
#ifdef USE_TAGLIB
  #include "rgtag.h"
#endif

#define CHECK_ERROR(condition, message, errorcode, goto_point)                 \
  if ((condition)) {                                                           \
    fprintf(stderr, message);                                                  \
    errcode = (errorcode);                                                     \
    goto goto_point;                                                           \
  }

extern long nproc();

struct gain_data {
  GArray* file_names;
  int calculate_lra, errcode;
#ifdef USE_TAGLIB
  char* tag_rg;                    /* NULL, "album" or "track" */
#if EBUR128_USE_SPEEX_RESAMPLER
  int tag_true_peak;
#endif
#endif
  int recursive_scan;
  char* peak;
  double interval;
  int mode;
  ebur128_state** library_states;
  double* segment_loudness;
  double* segment_lra;
  double* segment_peaks;
  double* segment_true_peaks;
};

/* Calculates gain and peak of i-th file in the gain_data struct.
 * This function will be in a GThreadPool. */
void calculate_gain_of_file(void* user, void* user_data) {
  struct gain_data* gd = (struct gain_data*) user_data;
  size_t i = (size_t) user - 1, j;

  size_t nr_frames_read, nr_frames_read_all = 0;

  ebur128_state* st = NULL;
  float* buffer = NULL;

  int errcode, result;
  FILE* file;

  struct input_ops* ops = NULL;
  struct input_handle* ih = NULL;

  gd->segment_loudness[i] = 0.0 / 0.0;

  ops = input_get_ops(g_array_index(gd->file_names, char*, i));
  if (!ops) {
    gd->errcode = 1;
    return;
  }
  ih = ops->handle_init();

  file = g_fopen(g_array_index(gd->file_names, char*, i), "rb");
  if (!file) {
    errcode = 1;
    goto endloop;
  }
  result = ops->open_file(ih, file);
  if (result) {
    errcode = 1;
    goto endloop;
  }
#ifdef USE_TAGLIB
  if (gd->tag_rg && ops->get_channels(ih) > 2) {
    fprintf(stderr, "ReplayGain tagging support only up to 2 channels!\n");
    errcode = 1;
    goto close_file;
  }
#endif

  st = ebur128_init(ops->get_channels(ih),
                    ops->get_samplerate(ih),
                    EBUR128_MODE_I |
                    (gd->calculate_lra ? EBUR128_MODE_LRA : 0) |
                    ((
                  #ifdef USE_TAGLIB
                     (gd->tag_rg && !gd->tag_true_peak) ||
                  #endif
                     (gd->peak && (!strcmp(gd->peak, "sample") ||
                                   !strcmp(gd->peak, "all")))) ?
                     EBUR128_MODE_SAMPLE_PEAK : 0)
                  #if EBUR128_USE_SPEEX_RESAMPLER
                  | ((
                  #ifdef USE_TAGLIB
                     (gd->tag_rg && gd->tag_true_peak) ||
                  #endif
                     (gd->peak && (!strcmp(gd->peak, "true") ||
                                   !strcmp(gd->peak, "dbtp") ||
                                   !strcmp(gd->peak, "all")))) ?
                     EBUR128_MODE_TRUE_PEAK : 0)
                  #endif
                    );
  CHECK_ERROR(!st, "Could not initialize EBU R128!\n", 1, close_file)
  gd->library_states[i] = st;

  /* Special case seq-3341-6-5channels-16bit.wav.
   * Set channel map with function ebur128_set_channel. */
  result = ops->set_channel_map(ih, st);
  if (result && st->channels == 5) {
    ebur128_set_channel(st, 0, EBUR128_LEFT);
    ebur128_set_channel(st, 1, EBUR128_RIGHT);
    ebur128_set_channel(st, 2, EBUR128_CENTER);
    ebur128_set_channel(st, 3, EBUR128_LEFT_SURROUND);
    ebur128_set_channel(st, 4, EBUR128_RIGHT_SURROUND);
  }
#ifdef USE_TAGLIB
  if (gd->tag_rg && st->channels == 1) {
    ebur128_set_channel(st, 0, EBUR128_DUAL_MONO);
  }
#endif

  result = ops->allocate_buffer(ih);
  CHECK_ERROR(result, "Could not allocate memory!\n", 1, free_buffer)
  gd->segment_peaks[i] = 0.0;
  buffer = ops->get_buffer(ih);

  while ((nr_frames_read = ops->read_frames(ih))) {
    nr_frames_read_all += nr_frames_read;
    result = ebur128_add_frames_float(st, buffer, (size_t) nr_frames_read);
    CHECK_ERROR(result, "Internal EBU R128 error!\n", 1, free_buffer)
  }
  if (ops->check_ok(ih, nr_frames_read_all)) {
    fprintf(stderr, "Warning: Could not read full file"
                            " or determine right length!\n");
  }

  if ((st->mode & EBUR128_MODE_SAMPLE_PEAK) == EBUR128_MODE_SAMPLE_PEAK) {
    for (j = 0; j < st->channels; ++j) {
      if (ebur128_sample_peak(st, j) > gd->segment_peaks[i]) {
        gd->segment_peaks[i] = ebur128_sample_peak(st, j);
      }
    }
  }
#if EBUR128_USE_SPEEX_RESAMPLER
  if ((st->mode & EBUR128_MODE_TRUE_PEAK) == EBUR128_MODE_TRUE_PEAK) {
    for (j = 0; j < st->channels; ++j) {
      if (ebur128_true_peak(st, j) > gd->segment_true_peaks[i]) {
        gd->segment_true_peaks[i] = ebur128_true_peak(st, j);
      }
    }
  }
#endif
  gd->segment_loudness[i] = ebur128_loudness_global(st);
  if (gd->calculate_lra) {
    gd->segment_lra[i] = ebur128_loudness_range(st);
  }
  fprintf(stderr, "*");

free_buffer:
  ops->free_buffer(ih);

close_file:
  ops->close_file(ih, file);

endloop:
  ops->handle_destroy(&ih);
  gd->errcode = errcode;
}

void print_gain_value(double x) {
  if (isnan(x)) {
    printf("nan");
  } else if (isinf(x) && x > 0.0) {
    printf("inf");
  } else if (isinf(x) && x < 0.0) {
    printf("-inf");
  } else {
    printf("%.1f", x);
  }
}

void print_file_result(struct gain_data* gd, size_t i) {
  fprintf(stderr, "\r");
  print_gain_value(gd->segment_loudness[i]);
  fflush(stdout);
  fprintf(stderr, " LUFS");
  if (gd->calculate_lra) {
    printf(",");
    fflush(stdout);
    fprintf(stderr, " LRA: ");
    printf("%.1f", gd->segment_lra[i]);
    fflush(stdout);
    fprintf(stderr, " LU");
  }
  if (gd->peak &&
      (!strcmp(gd->peak, "sample") ||
       !strcmp(gd->peak, "all"))) {
    printf(",");
    fflush(stdout);
    fprintf(stderr, " sample peak: ");
    printf("%.8f", gd->segment_peaks[i]);
    fflush(stdout);
  }
  if (gd->peak &&
      (!strcmp(gd->peak, "true") ||
       !strcmp(gd->peak, "all"))) {
    printf(",");
    fflush(stdout);
    fprintf(stderr, " true peak: ");
    printf("%.8f", gd->segment_true_peaks[i]);
    fflush(stdout);
  }
  if (gd->peak &&
      (!strcmp(gd->peak, "dbtp") ||
       !strcmp(gd->peak, "all"))) {
    double tp_gain = 20.0 * log(gd->segment_true_peaks[i]) / log(10.0);
    printf(",");
    fflush(stdout);
    fprintf(stderr, " true peak: ");
    print_gain_value(tp_gain);
    fflush(stdout);
    fprintf(stderr, " dBTP");
  }
  printf(",");
  fflush(stdout);
  fprintf(stderr, " ");
  {
    char* fn;
#ifdef G_OS_WIN32
    fn = g_win32_locale_filename_from_utf8(
                              g_array_index(gd->file_names, char*, i));
#else
    fn = g_filename_from_utf8(g_array_index(gd->file_names, char*, i),
                              -1, NULL, NULL, NULL);
#endif
    printf("%s\n", fn);
    g_free(fn);
  }
}

void print_result(struct gain_data* gd, double gated_loudness) {
  size_t i;
  fprintf(stderr, "\r--------------------"
                    "--------------------"
                    "--------------------"
                    "--------------------\n");
  print_gain_value(gated_loudness);
  fflush(stdout);
  fprintf(stderr, " LUFS");

  if (gd->calculate_lra) {
    printf(",");
    fflush(stdout);
    fprintf(stderr, " LRA: ");
    printf("%.1f",
            ebur128_loudness_range_multiple(gd->library_states,
                                            gd->file_names->len));
    fflush(stdout);
    fprintf(stderr, " LU");
  }
  if (gd->peak &&
      (!strcmp(gd->peak, "sample") ||
       !strcmp(gd->peak, "all"))) {
    double max_peak = 0.0;
    for (i = 0; i < gd->file_names->len; ++i) {
      if (gd->segment_peaks[i] > max_peak) {
        max_peak = gd->segment_peaks[i];
      }
    }
    printf(",");
    fflush(stdout);
    fprintf(stderr, " sample peak: ");
    printf("%.8f", max_peak);
    fflush(stdout);
  }
  if (gd->peak &&
      (!strcmp(gd->peak, "true") ||
       !strcmp(gd->peak, "all"))) {
    double max_peak = 0.0;
    for (i = 0; i < gd->file_names->len; ++i) {
      if (gd->segment_true_peaks[i] > max_peak) {
        max_peak = gd->segment_true_peaks[i];
      }
    }
    printf(",");
    fflush(stdout);
    fprintf(stderr, " true peak: ");
    printf("%.8f", max_peak);
    fflush(stdout);
  }
  if (gd->peak &&
      (!strcmp(gd->peak, "dbtp") ||
       !strcmp(gd->peak, "all"))) {
    double max_peak = 0.0;
    for (i = 0; i < gd->file_names->len; ++i) {
      if (gd->segment_true_peaks[i] > max_peak) {
        max_peak = gd->segment_true_peaks[i];
      }
    }
    printf(",");
    fflush(stdout);
    fprintf(stderr, " true peak: ");
    print_gain_value(20.0 * log(max_peak) / log(10.0));
    fflush(stdout);
    fprintf(stderr, " dBTP");
  }
  printf("\n");
}

void tag_files(struct gain_data* gd, double gated_loudness) {
#ifdef USE_TAGLIB
  size_t i;
  double global_peak = 0.0;
  double* peaks = gd->tag_true_peak ? gd->segment_true_peaks :
                                      gd->segment_peaks;
  for (i = 0; i < gd->file_names->len; ++i) {
    if (peaks[i] > global_peak) {
      global_peak = peaks[i];
    }
  }
  for (i = 0; i < gd->file_names->len; ++i) {
    if (gd->library_states[i]) {
      char* fn;
#ifdef G_OS_WIN32
      fn = g_win32_locale_filename_from_utf8(
                                g_array_index(gd->file_names, char*, i));
#else
      fn = g_filename_from_utf8(g_array_index(gd->file_names, char*, i),
                                -1, NULL, NULL, NULL);
#endif
      set_rg_info(fn,
                  -18.0 - gd->segment_loudness[i],
                  peaks[i],
                  !strcmp(gd->tag_rg, "album"),
                  -18.0 - gated_loudness,
                  global_peak);
      g_free(fn);
    }
  }
#else
  (void) gd; (void) gated_loudness;
#endif
}

int loudness_or_lra(struct gain_data* gd) {
  double gated_loudness;
  int errcode = 0;
  size_t i;
  GThreadPool* pool;

  if (!gd->file_names->len) {
    return 2;
  }

  gd->segment_loudness =   calloc(gd->file_names->len, sizeof(double));
  gd->segment_lra =        calloc(gd->file_names->len, sizeof(double));
  gd->segment_peaks =      calloc(gd->file_names->len, sizeof(double));
  gd->segment_true_peaks = calloc(gd->file_names->len, sizeof(double));
  gd->library_states =     calloc(gd->file_names->len, sizeof(ebur128_state*));

  pool = g_thread_pool_new(calculate_gain_of_file, gd, (int) nproc(),
                           FALSE, NULL);

  fprintf(stderr, "\n");
  for (i = 0; i < gd->file_names->len; ++i) {
    g_thread_pool_push(pool, GINT_TO_POINTER(i + 1), NULL);
  }
  g_thread_pool_free(pool, FALSE, TRUE);
  for (i = 0; i < gd->file_names->len; ++i) {
    print_file_result(gd, i);
  }

  gated_loudness = ebur128_loudness_global_multiple(gd->library_states,
                                                    gd->file_names->len);
  print_result(gd, gated_loudness);
#ifdef USE_TAGLIB
  if (gd->tag_rg) {
    fprintf(stderr, "tagging...\n");
    tag_files(gd, gated_loudness);
  }
#endif

  fprintf(stderr, "\n");

  for (i = 0; i < gd->file_names->len; ++i) {
    if (gd->library_states[i]) {
      ebur128_destroy(&gd->library_states[i]);
    }
  }

  free(gd->library_states);
  free(gd->segment_loudness);
  free(gd->segment_lra);
  free(gd->segment_peaks);
  free(gd->segment_true_peaks);
  return errcode;
}

int scan_files_interval_loudness(struct gain_data* gd) {
  int errcode = 0, result;
  size_t i;
  ebur128_state* st = NULL;
  float* buffer = NULL;
  size_t nr_frames_read;
  size_t frames_counter = 0, frames_needed;
  FILE* file;

  struct input_ops* ops = NULL;
  struct input_handle* ih = NULL;

  for (i = 0; i < gd->file_names->len; ++i) {
    ops = input_get_ops(g_array_index(gd->file_names, char*, i));
    if (!ops) {
      continue;
    }
    ih = ops->handle_init();

    file = g_fopen(g_array_index(gd->file_names, char*, i), "rb");
    if (!file) {
      errcode = 1;
      goto endloop;
    }
    result = ops->open_file(ih, file);
    if (result) {
      errcode = 1;
      goto endloop;
    }

    if (!st) {
      st = ebur128_init(ops->get_channels(ih),
                        ops->get_samplerate(ih),
                        gd->mode);
      CHECK_ERROR(!st, "Could not initialize EBU R128!\n", 1, close_file)
    } else {
      if (!ebur128_change_parameters(st, ops->get_channels(ih),
                                         ops->get_samplerate(ih))) {
        frames_counter = 0;
      }
    }

    /* Special case seq-3341-6-5channels-16bit.wav.
     * Set channel map with function ebur128_set_channel. */
    result = ops->set_channel_map(ih, st);
    if (result && st->channels == 5) {
      ebur128_set_channel(st, 0, EBUR128_LEFT);
      ebur128_set_channel(st, 1, EBUR128_RIGHT);
      ebur128_set_channel(st, 2, EBUR128_CENTER);
      ebur128_set_channel(st, 3, EBUR128_LEFT_SURROUND);
      ebur128_set_channel(st, 4, EBUR128_RIGHT_SURROUND);
    }

    frames_needed = (size_t) (gd->interval * (double) st->samplerate + 0.5);

    result = ops->allocate_buffer(ih);
    CHECK_ERROR(result, "Could not allocate memory!\n", 1, close_file)
    buffer = ops->get_buffer(ih);
    while ((nr_frames_read = ops->read_frames(ih))) {
      float* tmp_buffer = buffer;
      while (nr_frames_read > 0) {
        if (frames_counter + nr_frames_read >= frames_needed) {
          result = ebur128_add_frames_float(st, tmp_buffer,
                                            frames_needed - frames_counter);
          CHECK_ERROR(result, "Internal EBU R128 error!\n", 1, free_buffer)
          tmp_buffer += (frames_needed - frames_counter) * st->channels;
          nr_frames_read -= frames_needed - frames_counter;
          frames_counter = 0;
          switch (gd->mode) {
            case EBUR128_MODE_M:
              printf("%f\n", ebur128_loudness_momentary(st));
              break;
            case EBUR128_MODE_S:
              printf("%f\n", ebur128_loudness_shortterm(st));
              break;
            case EBUR128_MODE_I:
              printf("%f\n", ebur128_loudness_global(st));
              break;
            default:
              fprintf(stderr, "Invalid mode!\n");
              goto free_buffer;
              break;
          }
        } else {
          result = ebur128_add_frames_float(st, tmp_buffer, nr_frames_read);
          CHECK_ERROR(result, "Internal EBU R128 error!\n", 1, free_buffer)
          tmp_buffer += (nr_frames_read) * st->channels;
          frames_counter += nr_frames_read;
          nr_frames_read = 0;
        }
      }
    }
  free_buffer:
    ops->free_buffer(ih);
  close_file:
    ops->close_file(ih, file);
  endloop:
    ops->handle_destroy(&ih);
  }
  if (st) {
    ebur128_destroy(&st);
  }
  return errcode;
}

int scan_files_gated_loudness_or_lra(struct gain_data* gdt, int depth) {
  int errcode = 0;
  size_t i;
  GArray* regular_files = g_array_new(FALSE, TRUE, sizeof(char*));
  for (i = 0; i < gdt->file_names->len; ++i) {
    const char* fn = g_array_index(gdt->file_names, char*, i);
    if (g_file_test(fn, G_FILE_TEST_IS_REGULAR)) {
      char* foo = g_strdup(fn);
      g_array_append_val(regular_files, foo);
    } else if (depth && g_file_test(fn, G_FILE_TEST_IS_DIR)) {
      GArray* files_in_new_dir = g_array_new(FALSE, TRUE, sizeof(char*));
      GDir* dir = g_dir_open(fn, 0, NULL);
      const char* dir_file = NULL;
      while ((dir_file = g_dir_read_name(dir))) {
        char* foo = g_build_filename(fn, dir_file, NULL);
        g_array_append_val(files_in_new_dir, foo);
      }
      {
        GArray* old_file_names = gdt->file_names;
        gdt->file_names = files_in_new_dir;
        errcode = scan_files_gated_loudness_or_lra(gdt, depth - 1);
        gdt->file_names = old_file_names;
      }
    }
  }
  {
    GArray* old_file_names = gdt->file_names;
    gdt->file_names = regular_files;
    errcode = loudness_or_lra(gdt);
    gdt->file_names = old_file_names;
  }

  return errcode;
}

static struct gain_data gd;

static gboolean parse_interval(const gchar *option_name,
                               const gchar *value,
                               gpointer data,
                               GError **error) {
  (void) data; (void) error;
  if (gd.mode) {
    fprintf(stderr, "-m, -s and -i can not be specified together!\n");
    return FALSE;
  }
  gd.interval = atof(value);
  if (gd.interval <= 0.0) {
    return FALSE;
  }
  if (!strcmp(option_name, "-m") ||
      !strcmp(option_name, "--momentary")) {
    gd.mode = EBUR128_MODE_M;
    if (gd.interval > 0.4) {
      fprintf(stderr, "Warning: you may lose samples when specifying "
                      "this interval!\n");
    }
  } else if (!strcmp(option_name, "-s") ||
             !strcmp(option_name, "--shortterm")) {
    gd.mode = EBUR128_MODE_S;
    if (gd.interval > 3.0) {
      fprintf(stderr, "Warning: you may lose samples when specifying "
                      "this interval!\n");
    }
  } else if (!strcmp(option_name, "-i") ||
             !strcmp(option_name, "--integrated")) {
    gd.mode = EBUR128_MODE_I;
  } else {
    return FALSE;
  }
  return TRUE;
}

static char** file_names = NULL;
static char* forced_plugin = NULL;

#ifdef USE_TAGLIB
static GOptionEntry tagging_entries[] = {
  { "tagging", 't', 0, G_OPTION_ARG_STRING,
                 &gd.tag_rg,
                 "write ReplayGain tags to files"
                 "\n                                        "
                 "(reference: -18 LUFS)"
                 "\n                                        "
                 "-t album: write album gain"
                 "\n                                        "
                 "-t track: write track gain", "album|track" },
#if EBUR128_USE_SPEEX_RESAMPLER
  { "tag-tp", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE,
                 &gd.tag_true_peak,
                 0, NULL },
#endif
  { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, 0 }
};
#endif

static GOptionEntry r128_entries[] = {
  { "momentary", 'm', 0, G_OPTION_ARG_CALLBACK,
                 (void*) (size_t) &parse_interval,
                 "print momentary loudness every INTERVAL"
                 "\n                                        "
                 "seconds", "INTERVAL" },
  { "shortterm", 's', 0, G_OPTION_ARG_CALLBACK,
                 (void*) (size_t) &parse_interval,
                 "print shortterm loudness every INTERVAL"
                 "\n                                        "
                 "seconds", "INTERVAL" },
  { "integrated", 'i', 0, G_OPTION_ARG_CALLBACK,
                 (void*) (size_t) &parse_interval,
                 "print integrated loudness (from start of"
                 "\n                                        "
                 "file) every INTERVAL seconds", "INTERVAL" },
  { "lra", 'l', 0, G_OPTION_ARG_NONE,
                 &gd.calculate_lra,
                 "calculate loudness range in LRA", NULL },
  { "peak", 'p', 0, G_OPTION_ARG_STRING,
                 &gd.peak,
                 "display peak values"
                 "\n                                        "
                 "-p sample: sample peak (float value)"
#if EBUR128_USE_SPEEX_RESAMPLER
                 "\n                                        "
                 "-p true:   true peak (float value)"
                 "\n                                        "
                 "-p dbtp:   true peak (dB True Peak)"
                 "\n                                        "
                 "-p all:    show all peak values"
#endif
                 ,
#if EBUR128_USE_SPEEX_RESAMPLER
                 "sample|true|dbtp|all"
#else
                 "sample              "
#endif
                 },
  { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, 0 }
};

static GOptionEntry entries[] = {
  { "recursive", 'r', 0, G_OPTION_ARG_NONE,
                 &gd.recursive_scan,
                 "scan directory recursively, one album"
                 "\n                                        "
                 "per folder", NULL },
  { "force-plugin", 0, 0, G_OPTION_ARG_STRING,
                 &forced_plugin,
                 "force input plugin; PLUGIN is one of:"
                 "\n                                        "
                 "sndfile, mpg123, musepack, ffmpeg", "PLUGIN" },
  { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY,
                 &file_names,
                 "<input>" , "[FILE|DIRECTORY]..."},
  { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, 0 }
};

int test_files_in_gd(struct gain_data* gdata, size_t ac, int test) {
  int errcode = 0;
  size_t i;
  for (i = 0; i < ac; ++i) {
    if (!g_file_test(g_array_index(gdata->file_names, char*, i),
                     test)) {
      errcode = 1;
      switch (test) {
        case G_FILE_TEST_EXISTS:
          fprintf(stderr, "File or directory %s does not exist!\n",
                          g_array_index(gdata->file_names, char*, i));
          break;
        case G_FILE_TEST_IS_DIR:
          fprintf(stderr, "%s is not a directory!\n",
                          g_array_index(gdata->file_names, char*, i));
          break;
        case G_FILE_TEST_IS_REGULAR:
          fprintf(stderr, "%s is not a regular file!\n",
                          g_array_index(gdata->file_names, char*, i));
          break;
        default:
          return 2;
      }
    }
  }
  return errcode;
}

int main(int ac, char* av[]) {
  int errcode = 0;
  size_t i = 0, nr_files = 0;
  GError* error = NULL;
  GOptionContext* context;
#ifdef USE_TAGLIB
  GOptionGroup* tagging_group;
#endif
  GOptionGroup* r128_group;

  g_thread_init(NULL);

  gd.calculate_lra = 0;
#ifdef USE_TAGLIB
  gd.tag_rg = NULL;
#endif
  gd.interval = 0.0;
  gd.mode = 0;
  gd.file_names = NULL;
  gd.recursive_scan = FALSE;
  gd.peak = NULL;

  context = g_option_context_new("- analyse loudness of audio files");
  g_option_context_add_main_entries(context, entries, NULL);

  /* add tagging options */
#ifdef USE_TAGLIB
  tagging_group = g_option_group_new("tagging",
                                     "ReplayGain tagging options",
                                     "Show tagging help options",
                                     NULL, NULL);
  g_option_group_add_entries(tagging_group, tagging_entries);
  g_option_context_add_group(context, tagging_group);
#endif
  /* add r128 options */
  r128_group = g_option_group_new("r128",
                                  "R128/BS.1770-1 options",
                                  "Show R128/BS.1770-1 help options",
                                  NULL, NULL);
  g_option_group_add_entries(r128_group, r128_entries);
  g_option_context_add_group(context, r128_group);


  g_option_context_parse(context, &ac, &av, &error);

  if (input_init(forced_plugin)) {
    return 1;
  }

  if (error) {
    fprintf(stderr, "%s\n", error->message);
    return 1;
  }

#ifdef USE_TAGLIB
  if (gd.tag_rg &&
      strcmp(gd.tag_rg, "album") &&
      strcmp(gd.tag_rg, "track")) {
    fprintf(stderr, "Invalid argument to --tagging!\n");
    return 1;
  }
#if EBUR128_USE_SPEEX_RESAMPLER
  if (gd.tag_true_peak && !gd.tag_rg) {
    fprintf(stderr, "Please specify a tagging option to use!\n");
    return 1;
  }
#endif
#endif

  if (gd.peak &&
#if EBUR128_USE_SPEEX_RESAMPLER
      strcmp(gd.peak, "all") &&
      strcmp(gd.peak, "true") &&
      strcmp(gd.peak, "dbtp") &&
#endif
      strcmp(gd.peak, "sample")) {
    fprintf(stderr, "Invalid argument to --peak!\n");
    return 1;
  }

  if (!file_names) {
#if GLIB_CHECK_VERSION(2, 14, 0)
    gchar* help = g_option_context_get_help(context, FALSE, NULL);
    fprintf(stderr, "%s", help);
#else
    fprintf(stderr, "Get help with -h or --help.\n");
#endif
    g_option_context_free(context);
    return 1;
  }
  g_option_context_free(context);

  /* Put all filenames in the file name GArray in gd. */
  nr_files = g_strv_length(file_names);
  gd.file_names = g_array_new(FALSE, TRUE, sizeof(char*));
  for (i = 0; i < nr_files; ++i) {
    char* fn = g_strdup(file_names[i]);
    g_array_append_val(gd.file_names, fn);
  }

  if (gd.interval > 0.0) {
    if (test_files_in_gd(&gd, nr_files, G_FILE_TEST_IS_REGULAR)) {
      return 1;
    } else {
      errcode = scan_files_interval_loudness(&gd);
    }
  } else {
    if (test_files_in_gd(&gd, nr_files, G_FILE_TEST_EXISTS)) {
      return 1;
    } else {
      errcode = scan_files_gated_loudness_or_lra(&gd, gd.recursive_scan ? -1 : 1);
    }
  }

  /* Free the file name GArray in gd. */
  for (i = 0; i < gd.file_names->len; ++i) {
    g_free(g_array_index(gd.file_names, char*, i));
  }
  g_array_free(gd.file_names, TRUE);

  input_deinit();

  return errcode;
}

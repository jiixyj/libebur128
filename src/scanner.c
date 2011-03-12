/* See LICENSE file for copyright and license details. */
#define _POSIX_C_SOURCE 200112L
#include <float.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <glib.h>
#include <glib/gstdio.h>

#ifdef G_OS_WIN32
  #define isnan(x) _isnan(x)
  #define isinf(x) (!isnan(x) && isnan((x) - (x)))
#endif

#include "ebur128.h"
#include "input.h"
#include "rgtag.h"

#define OUTSIDE_SPEEX
#define RANDOM_PREFIX ebur128
#include "speex_resampler.h"

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
  char* tag_rg;                    /* NULL, "album" or "track" */
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
  size_t i = (size_t) user - 1;

  struct input_handle* ih = input_handle_init();
  size_t nr_frames_read, nr_frames_read_all = 0;

  ebur128_state* st = NULL;
  float* buffer = NULL;
  SpeexResamplerState* resampler = NULL;
  size_t resampler_buffer_frames = 0;
  float* resampler_buffer = NULL;
  size_t oversample_factor = 1;

  int errcode, result;
  FILE* file;

  gd->segment_loudness[i] = 0.0 / 0.0;
  file = g_fopen(g_array_index(gd->file_names, char*, i), "rb");
  if (!file) {
    errcode = 1;
    goto endloop;
  }
  result = input_open_file(ih, file);
  if (result) {
    errcode = 1;
    goto endloop;
  }

  st = ebur128_init(input_get_channels(ih),
                    input_get_samplerate(ih),
                    EBUR128_MODE_I |
                    (gd->calculate_lra ? EBUR128_MODE_LRA : 0));
  CHECK_ERROR(!st, "Could not initialize EBU R128!\n", 1, close_file)
  gd->library_states[i] = st;

  /* Special case seq-3341-6-5channels-16bit.wav.
   * Set channel map with function ebur128_set_channel. */
  result = input_set_channel_map(ih, st);
  if (result && st->channels == 5) {
    ebur128_set_channel(st, 0, EBUR128_LEFT);
    ebur128_set_channel(st, 1, EBUR128_RIGHT);
    ebur128_set_channel(st, 2, EBUR128_CENTER);
    ebur128_set_channel(st, 3, EBUR128_LEFT_SURROUND);
    ebur128_set_channel(st, 4, EBUR128_RIGHT_SURROUND);
  }

  if (gd->peak && (!strcmp(gd->peak, "true") ||
                   !strcmp(gd->peak, "dbtp") ||
                   !strcmp(gd->peak, "all")) &&
      input_get_samplerate(ih) < 192000) {
    oversample_factor = 2;
    if (input_get_samplerate(ih) < 96000)
      oversample_factor = 4;
    resampler = ebur128_resampler_init((spx_uint32_t) input_get_channels(ih),
                                       (spx_uint32_t) input_get_samplerate(ih),
                                       (spx_uint32_t) (input_get_samplerate(ih) *
                                                       oversample_factor),
                                       8,
                                       &result);
    CHECK_ERROR(!resampler, "Could not initialize resampler!\n", 1, close_file)
  }

  result = input_allocate_buffer(ih);
  CHECK_ERROR(result, "Could not allocate memory!\n", 1, destroy_resampler)
  gd->segment_peaks[i] = 0.0;
  buffer = input_get_buffer(ih);

  if (resampler) {
    resampler_buffer_frames = input_get_buffer_size(ih) /
                              input_get_channels(ih) *
                              oversample_factor;
    resampler_buffer = calloc(resampler_buffer_frames *
                              input_get_channels(ih) *
                              sizeof(float), 1);
    CHECK_ERROR(!resampler_buffer, "Could not allocate memory!\n", 1, free_buffer)
  }

  while ((nr_frames_read = input_read_frames(ih))) {
    size_t j;
    if (gd->tag_rg ||
        (gd->peak && (!strcmp(gd->peak, "sample") ||
                      !strcmp(gd->peak, "all")))) {
      for (j = 0; j < nr_frames_read * st->channels; ++j) {
        if (buffer[j] > gd->segment_peaks[i])
          gd->segment_peaks[i] = buffer[j];
        else if (-buffer[j] > gd->segment_peaks[i])
          gd->segment_peaks[i] = -buffer[j];
      }
    }
    if (resampler) {
      spx_uint32_t in_len = (spx_uint32_t) nr_frames_read;
      spx_uint32_t out_len = (spx_uint32_t) resampler_buffer_frames;
      ebur128_resampler_process_interleaved_float(
                          resampler,
                          buffer,
                          &in_len,
                          resampler_buffer,
                          &out_len);
      for (j = 0; j < out_len * st->channels; ++j) {
        if (resampler_buffer[j] > gd->segment_true_peaks[i])
          gd->segment_true_peaks[i] = resampler_buffer[j];
        else if (-resampler_buffer[j] > gd->segment_true_peaks[i])
          gd->segment_true_peaks[i] = -resampler_buffer[j];
      }
    } else if (gd->peak && (!strcmp(gd->peak, "true") ||
                            !strcmp(gd->peak, "dbtp") ||
                            !strcmp(gd->peak, "all"))) {
      if (gd->tag_rg || !strcmp(gd->peak, "all")) {
        gd->segment_true_peaks[i] = gd->segment_peaks[i];
      } else {
        for (j = 0; j < nr_frames_read * st->channels; ++j) {
          if (buffer[j] > gd->segment_true_peaks[i])
            gd->segment_true_peaks[i] = buffer[j];
          else if (-buffer[j] > gd->segment_true_peaks[i])
            gd->segment_true_peaks[i] = -buffer[j];
        }
      }
    }

    nr_frames_read_all += nr_frames_read;
    result = ebur128_add_frames_float(st, buffer, (size_t) nr_frames_read);
    CHECK_ERROR(result, "Internal EBU R128 error!\n", 1, free_resampler_buffer)
  }
  if (input_check_ok(ih, nr_frames_read_all)) {
    fprintf(stderr, "Warning: Could not read full file"
                            " or determine right length!\n");
  }

  gd->segment_loudness[i] = ebur128_loudness_global(st);
  if (gd->calculate_lra) {
    gd->segment_lra[i] = ebur128_loudness_range(st);
  }
  fprintf(stderr, "*");

free_resampler_buffer:
  free(resampler_buffer);

free_buffer:
  input_free_buffer(ih);

destroy_resampler:
  if (resampler) {
    ebur128_resampler_destroy(resampler);
  }

close_file:
  input_close_file(ih, file);

endloop:
  input_handle_destroy(&ih);
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

int loudness_or_lra(struct gain_data* gd) {
  int errcode = 0;
  size_t i;
  double gated_loudness;
  GThreadPool* pool;

  if (!gd->file_names->len) {
    return 2;
  }

  gd->segment_loudness = calloc(gd->file_names->len, sizeof(double));
  gd->segment_lra = calloc(gd->file_names->len, sizeof(double));
  gd->segment_peaks = calloc(gd->file_names->len, sizeof(double));
  gd->segment_true_peaks = calloc(gd->file_names->len, sizeof(double));
  gd->library_states = calloc(gd->file_names->len, sizeof(ebur128_state*));

  CHECK_ERROR(input_init_library(),
              "Could not initialize input library!", 1, exit)

  pool = g_thread_pool_new(calculate_gain_of_file, gd, (int) nproc(),
                           FALSE, NULL);

  fprintf(stderr, "\n");
  for (i = 0; i < gd->file_names->len; ++i) {
    g_thread_pool_push(pool, GINT_TO_POINTER(i + 1), NULL);
  }
  g_thread_pool_free(pool, FALSE, TRUE);
  for (i = 0; i < gd->file_names->len; ++i) {
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

  gated_loudness = ebur128_loudness_global_multiple(gd->library_states,
                                                    gd->file_names->len);
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

  if (gd->tag_rg) {
    double global_peak = 0.0;
    fprintf(stderr, "tagging...\n");
    for (i = 0; i < gd->file_names->len; ++i) {
      if (gd->segment_peaks[i] > global_peak) {
        global_peak = gd->segment_peaks[i];
      }
    }
    for (i = 0; i < gd->file_names->len; ++i) {
      if (gd->library_states[i]) {
        set_rg_info(g_array_index(gd->file_names, char*, i),
                    -18.0 - gd->segment_loudness[i],
                    gd->segment_peaks[i],
                    !strcmp(gd->tag_rg, "album") ? 1 : 0,
                    -18.0 - gated_loudness,
                    global_peak);
      }
    }
  }
  fprintf(stderr, "\n");

  for (i = 0; i < gd->file_names->len; ++i) {
    if (gd->library_states[i]) {
      ebur128_destroy(&gd->library_states[i]);
    }
  }
  input_exit_library();

exit:
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

  CHECK_ERROR(input_init_library(),
              "Could not initialize input library!", 1, exit)


  for (i = 0; i < gd->file_names->len; ++i) {
    struct input_handle* ih = input_handle_init();

    file = g_fopen(g_array_index(gd->file_names, char*, i), "rb");
    if (!file) {
      errcode = 1;
      goto endloop;
    }
    result = input_open_file(ih, file);
    if (result) {
      errcode = 1;
      goto endloop;
    }

    if (!st) {
      st = ebur128_init(input_get_channels(ih),
                        input_get_samplerate(ih),
                        gd->mode);
      CHECK_ERROR(!st, "Could not initialize EBU R128!\n", 1, close_file)
    } else {
      if (!ebur128_change_parameters(st, input_get_channels(ih),
                                         input_get_samplerate(ih))) {
        frames_counter = 0;
      }
    }

    /* Special case seq-3341-6-5channels-16bit.wav.
     * Set channel map with function ebur128_set_channel. */
    result = input_set_channel_map(ih, st);
    if (result && st->channels == 5) {
      ebur128_set_channel(st, 0, EBUR128_LEFT);
      ebur128_set_channel(st, 1, EBUR128_RIGHT);
      ebur128_set_channel(st, 2, EBUR128_CENTER);
      ebur128_set_channel(st, 3, EBUR128_LEFT_SURROUND);
      ebur128_set_channel(st, 4, EBUR128_RIGHT_SURROUND);
    }

    frames_needed = (size_t) (gd->interval * (double) st->samplerate + 0.5);

    result = input_allocate_buffer(ih);
    CHECK_ERROR(result, "Could not allocate memory!\n", 1, close_file)
    buffer = input_get_buffer(ih);
    while ((nr_frames_read = input_read_frames(ih))) {
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
    input_free_buffer(ih);
  close_file:
    input_close_file(ih, file);
  endloop:
    input_handle_destroy(&ih);
  }
  input_exit_library();
  if (st) {
    ebur128_destroy(&st);
  }
exit:
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
static char* relative_gate_string = NULL;
static GOptionEntry entries[] = {
  { "lra", 'l', 0, G_OPTION_ARG_NONE,
                 &gd.calculate_lra,
                 "calculate loudness range in LRA\n", NULL },
  { "momentary", 'm', 0, G_OPTION_ARG_CALLBACK,
                 (void*) (size_t) &parse_interval,
                 "print momentary loudness every INTERVAL"
                 "\n                                      "
                 "seconds\n", "INTERVAL" },
  { "shortterm", 's', 0, G_OPTION_ARG_CALLBACK,
                 (void*) (size_t) &parse_interval,
                 "print shortterm loudness every INTERVAL"
                 "\n                                      "
                 "seconds\n", "INTERVAL" },
  { "integrated", 'i', 0, G_OPTION_ARG_CALLBACK,
                 (void*) (size_t) &parse_interval,
                 "print integrated loudness (from start of"
                 "\n                                      "
                 "file) every INTERVAL seconds\n", "INTERVAL" },
  { "tagging", 't', 0, G_OPTION_ARG_STRING,
                 &gd.tag_rg,
                 "write ReplayGain tags to files"
                 "\n                                      "
                 "(reference: -18 LUFS)"
                 "\n                                      "
                 "-t album: write album gain"
                 "\n                                      "
                 "-t track: write track gain\n", "album|track" },
  { "recursive", 'r', 0, G_OPTION_ARG_NONE,
                 &gd.recursive_scan,
                 "scan directory recursively, one album"
                 "\n                                      "
                 "per folder\n", NULL },
  { "peak", 'p', 0, G_OPTION_ARG_STRING,
                 &gd.peak,
                 "display peak values"
                 "\n                                      "
                 "-p sample: sample peak (float value)"
                 "\n                                      "
                 "-p true:   true peak (float value)"
                 "\n                                      "
                 "-p dbtp:   true peak (dB True Peak)"
                 "\n                                      "
                 "-p all:    show all peak values\n", "sample|true|dbtp|all" },
  { "gate", 0, 0, G_OPTION_ARG_STRING,
                 &relative_gate_string,
                 "FOR TESTING ONLY: set relative gate (dB)", NULL },
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

extern double relative_gate;

int main(int ac, char* av[]) {
  int errcode = 0;
  size_t i = 0, nr_files = 0;
  GError *error = NULL;
  GOptionContext *context;

  gd.calculate_lra = 0;
  gd.tag_rg = NULL;
  gd.interval = 0.0;
  gd.mode = 0;
  gd.file_names = NULL;
  gd.recursive_scan = FALSE;
  gd.peak = NULL;

  context = g_option_context_new("- analyse loudness of audio files");
  g_option_context_add_main_entries(context, entries, NULL);
  g_option_context_parse(context, &ac, &av, &error);
  if (relative_gate_string) {
    relative_gate = atof(relative_gate_string);
    if (relative_gate != -8.0) {
      fprintf(stderr, "WARNING: Setting relative gate to non-standard value %.2f dB!\n", relative_gate);
    }
  }

  if (error) {
    fprintf(stderr, "%s\n", error->message);
    return 1;
  }

  if (gd.tag_rg &&
      strcmp(gd.tag_rg, "album") &&
      strcmp(gd.tag_rg, "track")) {
    fprintf(stderr, "Invalid argument to --tagging!\n");
    return 1;
  }

  if (gd.peak &&
      strcmp(gd.peak, "true") &&
      strcmp(gd.peak, "sample") &&
      strcmp(gd.peak, "dbtp") &&
      strcmp(gd.peak, "all")) {
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

  g_thread_init(NULL);
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

  return errcode;
}

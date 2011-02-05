/* See LICENSE file for copyright and license details. */
#define _POSIX_C_SOURCE 200112L
#include <float.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <glib.h>

#include "./ebur128.h"
#include "./input.h"

#define CHECK_ERROR(condition, message, errorcode, goto_point)                 \
  if ((condition)) {                                                           \
    fprintf(stderr, message);                                                  \
    errcode = (errorcode);                                                     \
    goto goto_point;                                                           \
  }

extern long nproc();

struct gain_data {
  char* const* file_names;
  int calculate_lra, tag_rg, errcode;
  double momentary_interval, shortterm_interval;
  ebur128_state** library_states;
  double* segment_loudness;
  double* segment_peaks;
};

void calculate_gain_of_file(void* user, void* user_data) {
  struct gain_data* gd = (struct gain_data*) user_data;
  size_t i = (size_t) user - 1;
  char* const* av = gd->file_names;
  double* segment_loudness = gd->segment_loudness;
  double* segment_peaks = gd->segment_peaks;
  int calculate_lra = gd->calculate_lra, tag_rg = gd->tag_rg;

  struct input_handle* ih = input_handle_init();
  size_t nr_frames_read, nr_frames_read_all = 0;

  ebur128_state* st = NULL;
  float* buffer = NULL;

  int errcode, result;

  segment_loudness[i] = 0.0 / 0.0;
  result = input_open_file(ih, av[i]);
  CHECK_ERROR(result, "Could not open file!\n", 1, endloop)

  st = ebur128_init(input_get_channels(ih),
                    input_get_samplerate(ih),
                    EBUR128_MODE_I |
                    (calculate_lra ? EBUR128_MODE_LRA : 0));
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

  result = input_allocate_buffer(ih);
  CHECK_ERROR(result, "Could not allocate memory!\n", 1, close_file)
  segment_peaks[i] = 0.0;
  buffer = input_get_buffer(ih);
  while ((nr_frames_read = input_read_frames(ih))) {
    if (tag_rg) {
      size_t j;
      for (j = 0; j < (size_t) nr_frames_read * st->channels; ++j) {
        if (buffer[j] > segment_peaks[i])
          segment_peaks[i] = buffer[j];
        else if (-buffer[j] > segment_peaks[i])
          segment_peaks[i] = -buffer[j];
      }
    }
    nr_frames_read_all += nr_frames_read;
    result = ebur128_add_frames_float(st, buffer, (size_t) nr_frames_read);
    CHECK_ERROR(result, "Internal EBU R128 error!\n", 1, free_buffer)
  }
  if (input_check_ok(ih, nr_frames_read_all)) {
    fprintf(stderr, "Warning: Could not read full file"
                            " or determine right length!\n");
  }

  segment_loudness[i] = ebur128_loudness_global(st);
  fprintf(stderr, "*");

free_buffer:
  input_free_buffer(ih);

close_file:
  input_close_file(ih);

endloop:
  input_handle_destroy(&ih);
  gd->errcode = errcode;
}

int loudness_or_lra(struct gain_data* gd, int no_files) {
  int errcode = 0, i, result;
  GThreadPool* pool;

  CHECK_ERROR(input_init_library(),
              "Could not initialize input library!", 1, exit)

  pool = g_thread_pool_new(calculate_gain_of_file, gd, (int) nproc(),
                           FALSE, NULL);

  for (i = 0; i < no_files; ++i) {
    g_thread_pool_push(pool, GINT_TO_POINTER(i + 1), NULL);
  }
  g_thread_pool_free(pool, FALSE, TRUE);
  if (no_files > 1) {
    for (i = 0; i < no_files; ++i) {
      if (!isnan(gd->segment_loudness[i])) {
        fprintf(stderr, "\r");
        fprintf(stderr, "segment %d: %.2f LUFS\n", (int) i + 1,
                        gd->segment_loudness[i]);
      }
    }
  }

  result = 1;
  for (i = 0; i < no_files; ++i) {
    if (!gd->library_states[i]) {
      result = 0;
    }
  }

  if (result) {
    double gated_loudness;
    gated_loudness = ebur128_loudness_global_multiple(gd->library_states,
                                                      (size_t) (no_files));
    fprintf(stderr, "\rglobal loudness: %.2f LUFS\n", gated_loudness);

    if (gd->calculate_lra) {
      fprintf(stderr, "LRA: %.2f\n",
              ebur128_loudness_range_multiple(gd->library_states,
                                              (size_t) (no_files)));
    }

    if (gd->tag_rg) {
      double global_peak = 0.0;
      for (i = 0; i < no_files; ++i) {
        if (gd->segment_peaks[i] > global_peak) {
          global_peak = gd->segment_peaks[i];
        }
      }
      for (i = 0; i < no_files; ++i) {
        printf("%.8f %.8f %.8f %.8f\n", -18.0 - gd->segment_loudness[i],
                                        gd->segment_peaks[i],
                                        -18.0 - gated_loudness,
                                        global_peak);
      }
    }
  }

  for (i = 0; i < no_files; ++i) {
    if (gd->library_states[i]) {
      ebur128_destroy(&gd->library_states[i]);
    }
  }
  input_exit_library();

exit:
  return errcode;
}

int interval_loudness(struct gain_data* gd, int no_files, int mode) {
  int errcode = 0, i, result;
  ebur128_state* st = NULL;
  float* buffer = NULL;
  size_t nr_frames_read;
  size_t frames_counter = 0, frames_needed;

  CHECK_ERROR(input_init_library(),
              "Could not initialize input library!", 1, exit)


  for (i = 0; i < no_files; ++i) {
    struct input_handle* ih = input_handle_init();

    result = input_open_file(ih, gd->file_names[i]);
    CHECK_ERROR(result, "Could not open file!\n", 1, endloop)

    if (!st) {
      st = ebur128_init(input_get_channels(ih),
                        input_get_samplerate(ih),
                        (size_t) mode);
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

    frames_needed = (size_t) (((mode == EBUR128_MODE_M) ?
                              gd->momentary_interval : gd->shortterm_interval) *
                              (double) st->samplerate + 0.5);

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
          printf("%f\n", (mode == EBUR128_MODE_M) ?
                                        ebur128_loudness_momentary(st)
                                      : ebur128_loudness_shortterm(st));
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
    input_close_file(ih);
  endloop:
    input_handle_destroy(&ih);
  }
  input_exit_library();
  ebur128_destroy(&st);
exit:
  return errcode;
}

int main(int ac, char* const av[]) {
  int errcode = 0, c;

  struct gain_data gd;
  gd.calculate_lra = 0;
  gd.tag_rg = 0;
  gd.momentary_interval = 0.0;
  gd.shortterm_interval = 0.0;

  g_thread_init(NULL);

  CHECK_ERROR(ac < 2, "usage: r128-test [-r] [-t] [-m|s INTERVAL] FILENAME(S) ...\n\n"
                      " -r: calculate loudness range in LRA\n"
                      " -m: display momentary loudness every INTERVAL seconds\n"
                      " -s: display shortterm loudness every INTERVAL seconds\n"
                      " -t: output ReplayGain tagging info\n", 1, exit)
  while ((c = getopt(ac, av, "trm:s:")) != -1) {
    switch (c) {
      case 't':
        gd.tag_rg = 1;
        break;
      case 'r':
        gd.calculate_lra = 1;
        break;
      case 'm':
        gd.momentary_interval = atof(optarg);
        if (gd.momentary_interval <= 0.0) {
          fprintf(stderr, "Invalid argument to -m!\n");
          return 1;
        } else if (gd.momentary_interval > 0.4) {
          fprintf(stderr, "Warning: you may lose samples when specifying "
                          "this interval!\n");
        }
        break;
      case 's':
        gd.shortterm_interval = atof(optarg);
        if (gd.shortterm_interval <= 0.0) {
          fprintf(stderr, "Invalid argument to -s!\n");
          return 1;
        } else if (gd.shortterm_interval > 3.0) {
          fprintf(stderr, "Warning: you may lose samples when specifying "
                          "this interval!\n");
        }
        break;
      default:
        return 1;
        break;
    }
  }
  if (gd.momentary_interval > 0.0 && gd.shortterm_interval > 0.0) {
    fprintf(stderr, "-m and -s can not be specified together!\n");
    return 1;
  }

  gd.file_names = &av[optind];
  if (gd.momentary_interval > 0.0 || gd.shortterm_interval > 0.0) {
    int mode = gd.momentary_interval > 0.0 ? EBUR128_MODE_M : EBUR128_MODE_S;
    interval_loudness(&gd, ac - optind, mode);
  } else {
    gd.segment_loudness = calloc((size_t) (ac - optind), sizeof(double));
    gd.segment_peaks = calloc((size_t) (ac - optind), sizeof(double));
    gd.library_states = calloc((size_t) (ac - optind), sizeof(ebur128_state*));

    if (loudness_or_lra(&gd, ac - optind)) {
      errcode = 1;
    }

    free(gd.library_states);
    free(gd.segment_loudness);
    free(gd.segment_peaks);
  }

exit:
  return errcode;
}

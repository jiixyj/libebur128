#include "scanner-common.h"

#include <stdlib.h>
#include <stdio.h>
#include <glib/gstdio.h>

#ifdef HAVE_CONFIG_USE_SPEEX_H
  #include "use_speex.h"
#endif

#ifdef HAVE_CONFIG_USE_SNDFILE_H
  #include "use_sndfile.h"
#endif

#ifdef USE_SNDFILE
  #include <sndfile.h>
#endif

extern gboolean verbose;

static struct file_data empty;

GMutex *progress_mutex = NULL;
GCond *progress_cond = NULL;
guint64 elapsed_frames = 0;
guint64 total_frames = 0;

void scanner_init_common(void)
{
    total_frames = 0;
    elapsed_frames = 0;
    if (!progress_mutex) {
        progress_mutex = g_mutex_new();
    }
    if (!progress_cond) {
        progress_cond = g_cond_new();
    }
}

void scanner_reset_common(void)
{
    g_mutex_lock(progress_mutex);
    total_frames = elapsed_frames = 0;
    g_cond_broadcast(progress_cond);
    g_mutex_unlock(progress_mutex);
}

int open_plugin(const char *raw, const char *display,
                struct input_ops **ops,
                struct input_handle **ih)
{
    int result;

    *ops = input_get_ops(raw);
    if (!(*ops)) {
        if (verbose) fprintf(stderr, "No plugin found for file '%s'\n", display);
        return 1;
    }
    *ih = (*ops)->handle_init();

    result = (*ops)->open_file(*ih, raw);
    if (result) {
        if (verbose) fprintf(stderr, "Error opening file '%s'\n", display);
        return 1;
    }
    return 0;
}

void init_and_get_number_of_frames(struct filename_list_node *fln, int *do_scan)
{
    struct file_data *fd;

    struct input_ops *ops = NULL;
    struct input_handle *ih = NULL;
    int result;

    fln->d = g_malloc(sizeof(struct file_data));
    memcpy(fln->d, &empty, sizeof empty);
    fd = (struct file_data *) fln->d;

    result = open_plugin(fln->fr->raw, fln->fr->display, &ops, &ih);
    if (result) {
        goto free;
    }

    *do_scan = TRUE;
    fd->number_of_frames = ops->get_total_frames(ih);
    g_mutex_lock(progress_mutex);
    total_frames += fd->number_of_frames;
    g_cond_broadcast(progress_cond);
    g_mutex_unlock(progress_mutex);

  free:
    if (!result) ops->close_file(ih);
    if (ih) ops->handle_destroy(&ih);
}

void init_state_and_scan_work_item(struct filename_list_node *fln, struct scan_opts *opts)
{
    struct file_data *fd = (struct file_data *) fln->d;

    struct input_ops* ops = NULL;
    struct input_handle* ih = NULL;
    int r128_mode = EBUR128_MODE_I;
    unsigned int i;
    int *channel_map;

    int result;
    float *buffer = NULL;
    size_t nr_frames_read;

#ifdef USE_SNDFILE
    SNDFILE *outfile = NULL;
#endif

    result = open_plugin(fln->fr->raw, fln->fr->display, &ops, &ih);
    if (result) {
        g_mutex_lock(progress_mutex);
        elapsed_frames += fd->number_of_frames;
        g_cond_broadcast(progress_cond);
        g_mutex_unlock(progress_mutex);
        goto free;
    }

    if (opts->lra)
        r128_mode |= EBUR128_MODE_LRA;
    if (opts->peak) {
        if (!strcmp(opts->peak, "sample") || !strcmp(opts->peak, "all"))
            r128_mode |= EBUR128_MODE_SAMPLE_PEAK;
#ifdef USE_SPEEX_RESAMPLER
        if (!strcmp(opts->peak, "true") || !strcmp(opts->peak, "dbtp") ||
            !strcmp(opts->peak, "all"))
            r128_mode |= EBUR128_MODE_TRUE_PEAK;
#endif
    }
    if (opts->histogram)
        r128_mode |= EBUR128_MODE_HISTOGRAM;

    fd->st = ebur128_init(ops->get_channels(ih),
                          ops->get_samplerate(ih),
                          r128_mode);

    channel_map = g_malloc(fd->st->channels * sizeof(int));
    if (!ops->set_channel_map(ih, channel_map)) {
        for (i = 0; i < fd->st->channels; ++i) {
            ebur128_set_channel(fd->st, i, channel_map[i]);
        }
    }
    free(channel_map);

    if (fd->st->channels == 1 && opts->force_dual_mono) {
        ebur128_set_channel(fd->st, 0, EBUR128_DUAL_MONO);
    }

    result = ops->allocate_buffer(ih);
    if (result) abort();
    buffer = ops->get_buffer(ih);

#ifdef USE_SNDFILE
    if (opts->decode_file) {
        SF_INFO sf_info;
        memset(&sf_info, '\0', sizeof sf_info);
        sf_info.samplerate = (int) fd->st->samplerate;
        sf_info.channels = (int) fd->st->channels;
        sf_info.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
        outfile = sf_open(opts->decode_file, SFM_WRITE, &sf_info);
        if (!outfile) {
            fprintf(stderr, "output file could not be opened\n");
            exit(EXIT_FAILURE);
        }
    }
#endif

    while ((nr_frames_read = ops->read_frames(ih))) {
        g_mutex_lock(progress_mutex);
        elapsed_frames += nr_frames_read;
        g_cond_broadcast(progress_cond);
        g_mutex_unlock(progress_mutex);
        fd->number_of_elapsed_frames += nr_frames_read;
        result = ebur128_add_frames_float(fd->st, buffer, nr_frames_read);
#ifdef USE_SNDFILE
        if (opts->decode_file) {
            if (sf_writef_float(outfile, buffer, (sf_count_t) nr_frames_read) != (sf_count_t) nr_frames_read)
                sf_perror(outfile);
        }
#endif
        if (result) abort();
    }

#ifdef USE_SNDFILE
    if (opts->decode_file) {
        sf_close(outfile);
    }
#endif

    if (fd->number_of_elapsed_frames != fd->number_of_frames) {
        if (verbose) {
            fprintf(stderr, "Warning: Could not read full file"
                            " or determine right length: "
                            "Expected: %lu Got: %lu",
                            fd->number_of_frames, fd->number_of_elapsed_frames);
        }
        g_mutex_lock(progress_mutex);
        total_frames = total_frames + fd->number_of_elapsed_frames - fd->number_of_frames;
        g_cond_broadcast(progress_cond);
        g_mutex_unlock(progress_mutex);
    }
    ebur128_loudness_global(fd->st, &fd->loudness);
    if (opts->lra) {
        result = ebur128_loudness_range(fd->st, &fd->lra);
        if (result) abort();
    }

    if ((fd->st->mode & EBUR128_MODE_SAMPLE_PEAK) == EBUR128_MODE_SAMPLE_PEAK) {
        for (i = 0; i < fd->st->channels; ++i) {
            double sp;
            ebur128_sample_peak(fd->st, i, &sp);
            if (sp > fd->peak) {
                fd->peak = sp;
            }
        }
    }
#ifdef USE_SPEEX_RESAMPLER
    if ((fd->st->mode & EBUR128_MODE_TRUE_PEAK) == EBUR128_MODE_TRUE_PEAK) {
        for (i = 0; i < fd->st->channels; ++i) {
            double tp;
            ebur128_true_peak(fd->st, i, &tp);
            if (tp > fd->true_peak) {
                fd->true_peak = tp;
            }
        }
    }
#endif
    fd->scanned = TRUE;

    if (ih) ops->free_buffer(ih);
  free:
    if (!result) ops->close_file(ih);
    if (ih) ops->handle_destroy(&ih);
}

void init_state_and_scan(gpointer work_item, GThreadPool *pool)
{
    g_thread_pool_push(pool, work_item, NULL);
}

void destroy_state(struct filename_list_node *fln, gpointer unused)
{
    struct file_data *fd = (struct file_data *) fln->d;

    (void) unused;
    if (fd->st) {
        ebur128_destroy(&fd->st);
    }
}

void get_state(struct filename_list_node *fln, GPtrArray *states)
{
    struct file_data *fd = (struct file_data *) fln->d;

    if (fd->scanned) {
       g_ptr_array_add(states, fd->st);
    }
}

void get_max_peaks(struct filename_list_node *fln, struct file_data *result)
{
    struct file_data *fd = (struct file_data *) fln->d;

    if (fd->scanned) {
        if (fd->peak > result->peak) result->peak = fd->peak;
        if (fd->true_peak > result->true_peak) result->true_peak = fd->true_peak;
    }
}

static gpointer print_progress_bar(gpointer p_started)
{
    int percent, bars, i;
    static char progress_bar[81];

    int *started = (int *) p_started;

    for (;;) {
        g_mutex_lock(progress_mutex);
        // signal calling thread that we are ready
        if (!*started) {
            *started = 1;
            g_cond_signal(progress_cond);
        }
        g_cond_wait(progress_cond, progress_mutex);
        if (total_frames) {
            bars = (int) (elapsed_frames * G_GUINT64_CONSTANT(72) / total_frames);
            percent = (int) (elapsed_frames * G_GUINT64_CONSTANT(100) / total_frames);
        } else {
            bars = percent = 0;
        }
        bars = CLAMP(bars, 0, 72);
        percent = CLAMP(percent, 0, 100);
        progress_bar[0] = '[';
        for (i = 1; i <= bars; ++i) {
            progress_bar[i] = '#';
        }
        for (; i < 73; ++i) {
            progress_bar[i] = ' ';
        }
        if (percent >= 0 && percent <= 100)
            sprintf(&progress_bar[73], "] %3d%%", percent);
        fprintf(stderr, "%s\r", progress_bar);
        if (total_frames == elapsed_frames) {
            g_mutex_unlock(progress_mutex);
            break;
        }
        g_mutex_unlock(progress_mutex);
    }
    return NULL;
}

void clear_line(void) {
    int i;
    for (i = 0; i < 80; ++i) {
        fputc(' ', stderr);
    }
    fputc('\r', stderr);
}

void process_files(GSList *files, struct scan_opts *opts) {
    GThreadPool *pool;
    GThread *progress_bar_thread;

    int started = 0;

    // Start the progress bar thread. It misuses progress_mutex and
    // progress_cond to signal when it is ready.
    g_mutex_lock(progress_mutex);
    progress_bar_thread = g_thread_create(print_progress_bar,
                                          &started, TRUE, NULL);
    while (!started)
        g_cond_wait(progress_cond, progress_mutex);
    g_mutex_unlock(progress_mutex);

    pool = g_thread_pool_new((GFunc) init_state_and_scan_work_item,
                             opts, nproc(), FALSE, NULL);
    g_slist_foreach(files, (GFunc) init_state_and_scan, pool);
    g_thread_pool_free(pool, FALSE, TRUE);
    g_thread_join(progress_bar_thread);
}

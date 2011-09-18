#include "scanner-common.h"

#include <stdlib.h>
#include <stdio.h>
#include <glib/gstdio.h>

extern gboolean verbose;

static struct file_data empty;

int open_plugin(const char *raw, const char *display,
                       struct input_ops **ops,
                       struct input_handle **ih,
                       FILE **file)
{
    int result;

    *ops = input_get_ops(raw);
    if (!(*ops)) {
        if (verbose) fprintf(stderr, "No plugin found for file '%s'\n", display);
        return 1;
    }
    *ih = (*ops)->handle_init();

    *file = g_fopen(raw, "rb");
    if (!(*file)) {
        if (verbose) fprintf(stderr, "Error opening file '%s'\n", display);
        return 1;
    }
    result = (*ops)->open_file(*ih, *file, raw);
    if (result) {
        if (verbose) fprintf(stderr, "Error opening file '%s'\n", display);
        fclose(*file);
        return 1;
    }
    return 0;
}

void init_and_get_number_of_frames(gpointer user, gpointer user_data)
{
    struct filename_list_node *fln = (struct filename_list_node *) user;
    struct file_data *fd;

    struct input_ops *ops = NULL;
    struct input_handle *ih = NULL;
    FILE *file = NULL;

    (void) user_data;
    fln->d = g_malloc(sizeof(struct file_data));
    memcpy(fln->d, &empty, sizeof empty);
    fd = (struct file_data *) fln->d;

    fd->mutex = g_mutex_new();

    if (open_plugin(fln->fr->raw, fln->fr->display, &ops, &ih, &file)) {
        goto free;
    }

    fd->number_of_frames = ops->get_total_frames(ih);

  free:
    if (file) ops->close_file(ih, file);
    if (ih) ops->handle_destroy(&ih);
}

void sum_frames(gpointer user, gpointer user_data)
{
    struct filename_list_node *fln = (struct filename_list_node *) user;
    struct file_data *fd = (struct file_data *) fln->d;
    guint64 *fc = (guint64 *) user_data;

    g_mutex_lock(fd->mutex);
    fc[0] += fd->number_of_elapsed_frames;
    fc[1] += fd->number_of_frames;
    g_mutex_unlock(fd->mutex);
}


void init_state_and_scan_work_item(gpointer user, gpointer user_data)
{
    struct filename_list_node *fln = (struct filename_list_node *) user;
    struct file_data *fd = (struct file_data *) fln->d;
    struct scan_opts *opts = (struct scan_opts *) user_data;

    struct input_ops* ops = NULL;
    struct input_handle* ih = NULL;
    FILE *file = NULL;
    int r128_mode = EBUR128_MODE_I;
    unsigned int i;

    int result;
    float *buffer = NULL;
    size_t nr_frames_read;

    if (open_plugin(fln->fr->raw, fln->fr->display, &ops, &ih, &file)) {
        goto free;
    }

    if (opts->lra)
        r128_mode |= EBUR128_MODE_LRA;
    if (opts->peak) {
        if (!strcmp(opts->peak, "sample") || !strcmp(opts->peak, "all"))
            r128_mode |= EBUR128_MODE_SAMPLE_PEAK;
        if (!strcmp(opts->peak, "true") || !strcmp(opts->peak, "dbtp") ||
            !strcmp(opts->peak, "all"))
            r128_mode |= EBUR128_MODE_TRUE_PEAK;
    }

    fd->st = ebur128_init(ops->get_channels(ih),
                          ops->get_samplerate(ih),
                          r128_mode);

    result = ops->allocate_buffer(ih);
    if (result) abort();
    buffer = ops->get_buffer(ih);

    while ((nr_frames_read = ops->read_frames(ih))) {
        g_mutex_lock(fd->mutex);
        fd->number_of_elapsed_frames += nr_frames_read;
        g_mutex_unlock(fd->mutex);
        result = ebur128_add_frames_float(fd->st, buffer, nr_frames_read);
        if (result) abort();
    }
    g_mutex_lock(fd->mutex);
    if (fd->number_of_elapsed_frames != fd->number_of_frames) {
        if (verbose) fprintf(stderr, "Warning: Could not read full file"
                                     " or determine right length!\n");
        fd->number_of_frames = fd->number_of_elapsed_frames;
    }
    g_mutex_unlock(fd->mutex);
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
    if ((fd->st->mode & EBUR128_MODE_TRUE_PEAK) == EBUR128_MODE_TRUE_PEAK) {
        for (i = 0; i < fd->st->channels; ++i) {
            double tp;
            ebur128_true_peak(fd->st, i, &tp);
            if (tp > fd->true_peak) {
                fd->true_peak = tp;
            }
        }
    }
    fd->scanned = TRUE;

  free:
    if (ih) ops->free_buffer(ih);
    if (file) ops->close_file(ih, file);
    if (ih) ops->handle_destroy(&ih);
}

void init_state_and_scan(gpointer user, gpointer user_data)
{
    GThreadPool *pool = (GThreadPool *) user_data;
    g_thread_pool_push(pool, user, NULL);
}

void destroy_state(gpointer user, gpointer user_data)
{
    struct filename_list_node *fln = (struct filename_list_node *) user;
    struct file_data *fd = (struct file_data *) fln->d;

    (void) user_data;
    if (fd->st) {
        ebur128_destroy(&fd->st);
    }
    g_mutex_free(fd->mutex);
}

void get_state(gpointer user, gpointer user_data)
{
    struct filename_list_node *fln = (struct filename_list_node *) user;
    struct file_data *fd = (struct file_data *) fln->d;
    GPtrArray *states = (GPtrArray *) user_data;

    if (fd->scanned) {
       g_ptr_array_add(states, fd->st);
    }
}

void get_max_peaks(gpointer user, gpointer user_data)
{
    struct filename_list_node *fln = (struct filename_list_node *) user;
    struct file_data *fd = (struct file_data *) fln->d;
    struct file_data *result = (struct file_data *) user_data;

    if (fd->scanned) {
        if (fd->peak > result->peak) result->peak = fd->peak;
        if (fd->true_peak > result->true_peak) result->true_peak = fd->true_peak;
    }
}

gpointer print_progress_bar(gpointer data)
{
    GSList *files = (GSList *) data;
    guint64 fc[] = {0, 1};
    int percent, bars, i;
    static char progress_bar[81];

    while (fc[0] != fc[1]) {
        fc[0] = fc[1] = 0;
        g_slist_foreach(files, sum_frames, &fc);
        if (fc[1] == 0) break;
        bars = (int) (fc[0] * G_GUINT64_CONSTANT(72) / fc[1]);
        percent = (int) (fc[0] * G_GUINT64_CONSTANT(100) / fc[1]);
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
        g_usleep(G_USEC_PER_SEC / 10);
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

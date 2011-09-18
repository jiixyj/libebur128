#include "scanner-scan.h"

#include <glib/gstdio.h>
#include <stdlib.h>
#include <math.h>

#include "filetree.h"
#include "input.h"
#include "nproc.h"
#include "parse_args.h"

static struct file_data empty;

extern gboolean verbose;
static gboolean lra = FALSE;
static gchar *peak = NULL;

static GOptionEntry entries[] =
{
    { "lra", 'l', 0, G_OPTION_ARG_NONE, &lra, NULL, NULL },
    { "peak", 'p', 0, G_OPTION_ARG_STRING, &peak, NULL, NULL },
    { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, 0 }
};


static int open_plugin(const char *raw, const char *display,
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

static void init_and_get_number_of_frames(gpointer user, gpointer user_data)
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

static void sum_frames(gpointer user, gpointer user_data)
{
    struct filename_list_node *fln = (struct filename_list_node *) user;
    struct file_data *fd = (struct file_data *) fln->d;
    guint64 *fc = (guint64 *) user_data;

    g_mutex_lock(fd->mutex);
    fc[0] += fd->number_of_elapsed_frames;
    fc[1] += fd->number_of_frames;
    g_mutex_unlock(fd->mutex);
}


static void init_state_and_scan_work_item(gpointer user, gpointer user_data)
{
    struct filename_list_node *fln = (struct filename_list_node *) user;
    struct file_data *fd = (struct file_data *) fln->d;

    struct input_ops* ops = NULL;
    struct input_handle* ih = NULL;
    FILE *file = NULL;
    int r128_mode = EBUR128_MODE_I;
    unsigned int i;

    int result;
    float *buffer = NULL;
    size_t nr_frames_read;

    (void) user_data;
    if (open_plugin(fln->fr->raw, fln->fr->display, &ops, &ih, &file)) {
        goto free;
    }

    if (lra)
        r128_mode |= EBUR128_MODE_LRA;
    if (peak) {
        if (!strcmp(peak, "sample") || !strcmp(peak, "all"))
            r128_mode |= EBUR128_MODE_SAMPLE_PEAK;
        if (!strcmp(peak, "true") || !strcmp(peak, "dbtp") ||
            !strcmp(peak, "all"))
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
    if (lra) {
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

static void init_state_and_scan(gpointer user, gpointer user_data)
{
    GThreadPool *pool = (GThreadPool *) user_data;
    g_thread_pool_push(pool, user, NULL);
}

static void destroy_state(gpointer user, gpointer user_data)
{
    struct filename_list_node *fln = (struct filename_list_node *) user;
    struct file_data *fd = (struct file_data *) fln->d;

    (void) user_data;
    if (fd->st) {
        ebur128_destroy(&fd->st);
    }
    g_mutex_free(fd->mutex);
}


static void print_file_data(gpointer user, gpointer user_data)
{
    struct filename_list_node *fln = (struct filename_list_node *) user;
    struct file_data *fd = (struct file_data *) fln->d;

    (void) user_data;
    if (fd->scanned) {
        if (fd->loudness <= -HUGE_VAL) {
            g_print(" -inf LUFS, ");
        } else {
            g_print("%5.1f LUFS, ", fd->loudness);
        }
        if (lra) g_print("LRA: %4.1f LU, ", fd->lra);
        if (peak) {
            if (!strcmp(peak, "sample") || !strcmp(peak, "all"))
                g_print("sample peak: %.8f, ", fd->peak);
            if (!strcmp(peak, "true") || !strcmp(peak, "all"))
                g_print("true peak: %.8f, ", fd->true_peak);
            if (!strcmp(peak, "dbtp") || !strcmp(peak, "all"))
                if (fd->true_peak < DBL_MIN)
                    g_print("true peak:  -inf dBTP, ");
                else
                    g_print("true peak: %5.1f dBTP, ",
                            20.0 * log(fd->true_peak) / log(10.0));
        }
        print_utf8_string(fln->fr->display);
        putchar('\n');
    }
}

static void get_state(gpointer user, gpointer user_data)
{
    struct filename_list_node *fln = (struct filename_list_node *) user;
    struct file_data *fd = (struct file_data *) fln->d;
    GPtrArray *states = (GPtrArray *) user_data;

    if (fd->scanned) {
       g_ptr_array_add(states, fd->st);
    }
}

static void get_max_peaks(gpointer user, gpointer user_data)
{
    struct filename_list_node *fln = (struct filename_list_node *) user;
    struct file_data *fd = (struct file_data *) fln->d;
    struct file_data *result = (struct file_data *) user_data;

    if (fd->scanned) {
        if (fd->peak > result->peak) result->peak = fd->peak;
        if (fd->true_peak > result->true_peak) result->true_peak = fd->true_peak;
    }
}

static void print_summary(GSList *files)
{
    int i;
    GPtrArray *states = g_ptr_array_new();
    struct filename_list_node n;
    struct filename_representations fr;
    struct file_data result;
    memcpy(&result, &empty, sizeof empty);

    g_slist_foreach(files, get_state, states);
    ebur128_loudness_global_multiple((ebur128_state **) states->pdata,
                                     states->len, &result.loudness);
    if (lra) {
        ebur128_loudness_range_multiple((ebur128_state **) states->pdata,
                                        states->len, &result.lra);
    }
    if (peak) {
        g_slist_foreach(files, get_max_peaks, &result);
    }

    result.scanned = TRUE;
    n.fr = &fr;
    n.fr->display = "";
    n.d = &result;
    for (i = 0; i < 79; ++i) { putchar('-'); }; putchar('\n');
    print_file_data(&n, NULL);

    g_ptr_array_free(states, TRUE);
}

static gpointer print_progress_bar(gpointer data)
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

void loudness_scan(GSList *files)
{
    GThreadPool *pool = g_thread_pool_new(init_state_and_scan_work_item,
                                          NULL, nproc(), FALSE, NULL);
    GThread *progress_bar_thread;

    g_slist_foreach(files, init_and_get_number_of_frames, NULL);
    g_slist_foreach(files, init_state_and_scan, pool);
    progress_bar_thread = g_thread_create(print_progress_bar,
                                          files, TRUE, NULL);
    g_thread_pool_free(pool, FALSE, TRUE);
    g_thread_join(progress_bar_thread);

    g_slist_foreach(files, print_file_data, NULL);
    print_summary(files);
    g_slist_foreach(files, destroy_state, NULL);

    g_free(peak);
}

gboolean loudness_scan_parse(int *argc, char **argv[])
{
    gboolean success = parse_mode_args(argc, argv, entries);
    if (!success) return FALSE;
    if (peak && strcmp(peak, "sample") && strcmp(peak, "true")
             && strcmp(peak, "dbtp") && strcmp(peak, "all")) {
        fprintf(stderr, "Invalid argument to --peak!\n");
        return FALSE;
    }
    return TRUE;
}

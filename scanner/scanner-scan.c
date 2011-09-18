#include "scanner-scan.h"

#include <glib/gstdio.h>
#include <stdlib.h>
#include <math.h>

#include "input.h"
#include "nproc.h"
#include "parse_args.h"
#include "scanner-common.h"

static struct file_data empty;

static gboolean lra = FALSE;
static gchar *peak = NULL;

static GOptionEntry entries[] =
{
    { "lra", 'l', 0, G_OPTION_ARG_NONE, &lra, NULL, NULL },
    { "peak", 'p', 0, G_OPTION_ARG_STRING, &peak, NULL, NULL },
    { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, 0 }
};

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

void loudness_scan(GSList *files)
{
    struct scan_opts opts = {lra, peak};
    GThreadPool *pool;
    GThread *progress_bar_thread;

    pool = g_thread_pool_new(init_state_and_scan_work_item,
                             &opts, nproc(), FALSE, NULL);
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
    if (peak && strcmp(peak, "sample") && strcmp(peak, "true")
             && strcmp(peak, "dbtp") && strcmp(peak, "all")) {
        fprintf(stderr, "Invalid argument to --peak!\n");
        return FALSE;
    }
    if (!success) {
        if (*argc == 1) fprintf(stderr, "Missing arguments\n");
        return FALSE;
    }
    return TRUE;
}

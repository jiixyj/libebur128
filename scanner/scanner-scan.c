#include "scanner-scan.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <glib/gstdio.h>

#include "input.h"
#include "nproc.h"
#include "parse_args.h"
#include "scanner-common.h"

#ifdef HAVE_CONFIG_USE_SPEEX_H
  #include "use_speex.h"
#endif

static struct file_data empty;

extern gboolean histogram;
static gboolean lra = FALSE;
static gchar *peak = NULL;
extern gchar *decode_to_file;

static GOptionEntry entries[] =
{
    { "lra", 'l', 0, G_OPTION_ARG_NONE, &lra, NULL, NULL },
    { "peak", 'p', 0, G_OPTION_ARG_STRING, &peak, NULL, NULL },
    { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, 0 }
};

static void print_file_data(struct filename_list_node *fln, gpointer unused)
{
    struct file_data *fd = (struct file_data *) fln->d;

    (void) unused;
    if (fd->scanned) {
        if (fd->loudness <= -HUGE_VAL) {
            g_print(" -inf LUFS");
        } else {
            g_print("%5.1f LUFS", fd->loudness);
        }
        if (lra) g_print(", %4.1f LU", fd->lra);
        if (peak) {
            if (!strcmp(peak, "sample") || !strcmp(peak, "all"))
                g_print(", %11.6f", fd->peak);
            if (!strcmp(peak, "true") || !strcmp(peak, "all"))
                g_print(", %11.6f", fd->true_peak);
            if (!strcmp(peak, "dbtp") || !strcmp(peak, "all")) {
                if (fd->true_peak < DBL_MIN)
                    g_print(",  -inf dBTP");
                else
                    g_print(", %5.1f dBTP",
                            20.0 * log(fd->true_peak) / log(10.0));
            }
        }
        if (fln->fr->display[0]) {
            g_print(", ");
            print_utf8_string(fln->fr->display);
        }
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

    g_slist_foreach(files, (GFunc) get_state, states);
    ebur128_loudness_global_multiple((ebur128_state **) states->pdata,
                                     states->len, &result.loudness);
    if (lra) {
        ebur128_loudness_range_multiple((ebur128_state **) states->pdata,
                                        states->len, &result.lra);
    }
    if (peak) {
        g_slist_foreach(files, (GFunc) get_max_peaks, &result);
    }

    result.scanned = TRUE;
    n.fr = &fr;
    n.fr->display = "";
    n.d = &result;
    for (i = 0; i < 79; ++i) { fputc('-', stderr); }; fputc('\n', stderr);
    print_file_data(&n, NULL);

    g_ptr_array_free(states, TRUE);
}

void loudness_scan(GSList *files)
{
    struct scan_opts opts = {lra, peak, histogram, FALSE, decode_to_file};
    int do_scan = FALSE;

    g_slist_foreach(files, (GFunc) init_and_get_number_of_frames, &do_scan);
    if (do_scan) {

        process_files(files, &opts);

        clear_line();
        fprintf(stderr, "  Loudness");
        if (lra) fprintf(stderr, ",     LRA");
        if (peak) {
            if (!strcmp(peak, "sample") || !strcmp(peak, "all"))
                fprintf(stderr, ", Sample peak");
            if (!strcmp(peak, "true") || !strcmp(peak, "all"))
                fprintf(stderr, ",   True peak");
            if (!strcmp(peak, "dbtp") || !strcmp(peak, "all"))
                fprintf(stderr, ",  True peak");
        }
        fprintf(stderr, "\n");

        g_slist_foreach(files, (GFunc) print_file_data, NULL);
        print_summary(files);
    }
    g_slist_foreach(files, (GFunc) destroy_state, NULL);
    scanner_reset_common();

    g_free(peak);
}

gboolean loudness_scan_parse(int *argc, char **argv[])
{
    gboolean success = parse_mode_args(argc, argv, entries);
    if (peak && strcmp(peak, "sample")
#ifdef USE_SPEEX_RESAMPLER
             && strcmp(peak, "true")
             && strcmp(peak, "dbtp")
             && strcmp(peak, "all")
#endif
            ) {
        fprintf(stderr, "Invalid argument to --peak!\n");
        return FALSE;
    }
    if (!success) {
        if (*argc == 1) fprintf(stderr, "Missing arguments\n");
        return FALSE;
    }
    return TRUE;
}

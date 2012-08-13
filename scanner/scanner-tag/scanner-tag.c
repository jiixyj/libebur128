#include "scanner-tag.h"

#include <stdlib.h>
#include <stdio.h>

#include "parse_args.h"
#include "scanner-common.h"
#include "nproc.h"
#include "rgtag.h"

static struct file_data empty;

extern gboolean verbose;
extern gboolean histogram;
static gboolean track = FALSE;
static gboolean dry_run = FALSE;
static gboolean tag_tp = FALSE;
extern gchar *decode_to_file;

static GOptionEntry entries[] =
{
    { "track", 't', 0, G_OPTION_ARG_NONE, &track, NULL, NULL },
    { "dry-run", 'n', 0, G_OPTION_ARG_NONE, &dry_run, NULL, NULL },
    { "tag-tp", 0, 0, G_OPTION_ARG_NONE, &tag_tp, NULL, NULL },
    { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, 0 }
};

double clamp_rg(double x) {
  if (x < -51.0) return -51.0;
  else if (x > 51.0) return 51.0;
  else return x;
}

static void fill_album_data(struct filename_list_node *fln, double *album_data)
{
    struct file_data *fd = (struct file_data *) fln->d;

    fd->gain_album = album_data[0];
    fd->peak_album = album_data[1];
}

static gchar *current_dir;
static GSList *files_in_current_dir;

static void calculate_album_gain_and_peak_last_dir(void)
{
    double album_data[] = {0.0, 0.0};
    GPtrArray *states = g_ptr_array_new();
    struct file_data result;
    memcpy(&result, &empty, sizeof empty);

    files_in_current_dir = g_slist_reverse(files_in_current_dir);
    g_slist_foreach(files_in_current_dir, (GFunc) get_state, states);
    ebur128_loudness_global_multiple((ebur128_state **) states->pdata,
                                     states->len, &album_data[0]);
    album_data[0] = clamp_rg(RG_REFERENCE_LEVEL - album_data[0]);
    g_slist_foreach(files_in_current_dir, (GFunc) get_max_peaks, &result);
    album_data[1] = tag_tp ? result.true_peak : result.peak;
    g_slist_foreach(files_in_current_dir, (GFunc) fill_album_data, album_data);

    g_ptr_array_free(states, TRUE);

    g_free(current_dir);
    current_dir = NULL;
    g_slist_free(files_in_current_dir);
    files_in_current_dir = NULL;
}

static void calculate_album_gain_and_peak(struct filename_list_node *fln, gpointer unused)
{
    gchar *dirname;

    (void) unused;
    dirname = g_path_get_dirname(fln->fr->raw);
    if (!current_dir) {
        current_dir = g_strdup(dirname);
    }
    if (!strcmp(current_dir, dirname)) {
        files_in_current_dir = g_slist_prepend(files_in_current_dir, fln);
    } else {
        calculate_album_gain_and_peak_last_dir();
        current_dir = g_strdup(dirname);
        files_in_current_dir = g_slist_prepend(files_in_current_dir, fln);
    }
    g_free(dirname);
}


static void print_file_data(struct filename_list_node *fln, gpointer unused)
{
    struct file_data *fd = (struct file_data *) fln->d;

    (void) unused;
    if (fd->scanned) {
        if (!track) {
            g_print("%7.2f dB, %7.2f dB, %10.6f, %10.6f",
                    fd->gain_album,
                    clamp_rg(RG_REFERENCE_LEVEL - fd->loudness),
                    fd->peak_album,
                    tag_tp ? fd->true_peak : fd->peak);
        } else {
            g_print("%7.2f dB, %10.6f",
                    clamp_rg(RG_REFERENCE_LEVEL - fd->loudness),
                    tag_tp ? fd->true_peak : fd->peak);
        }
        if (fln->fr->display[0]) {
            g_print(", ");
            print_utf8_string(fln->fr->display);
        }
        putchar('\n');
    }
}

static int tag_output_state = 0;
void tag_file(struct filename_list_node *fln, int *ret)
{
    struct file_data *fd = (struct file_data *) fln->d;
    if (fd->scanned) {
        int error;
        char *basename, *extension, *filename;
        struct gain_data gd = { clamp_rg(RG_REFERENCE_LEVEL - fd->loudness),
                                fd->peak,
                                !track,
                                fd->gain_album,
                                fd->peak_album };

        basename = g_path_get_basename(fln->fr->raw);
        extension = strrchr(basename, '.');
        if (extension) ++extension;
        else extension = "";
#ifdef G_OS_WIN32
        filename = (char *) g_utf8_to_utf16(fln->fr->raw, -1, NULL, NULL, NULL);
#else
        filename = g_strdup(fln->fr->raw);
#endif

        error = set_rg_info(filename, extension, &gd);
        if (error) {
            if (tag_output_state == 0) {
                fflush(stderr);
                fputc('\n', stderr);
                tag_output_state = 1;
            }
            g_message("Error tagging %s", fln->fr->display);
            *ret = EXIT_FAILURE;
        } else {
            fputc('.', stderr);
            tag_output_state = 0;
        }

        g_free(basename);
        g_free(filename);
    }
}

int scan_files(GSList *files) {
    struct scan_opts opts = {FALSE, tag_tp ? "true" : "sample", histogram,
                             TRUE, decode_to_file};
    int do_scan = 0;

    g_slist_foreach(files, (GFunc) init_and_get_number_of_frames, &do_scan);
    if (do_scan) {

        process_files(files, &opts);

        if (!track) {
            g_slist_foreach(files, (GFunc) calculate_album_gain_and_peak, NULL);
            calculate_album_gain_and_peak_last_dir();
        }

        clear_line();
        if (!track) {
            fprintf(stderr, "Album gain, Track gain, Album peak, Track peak\n");
        } else {
            fprintf(stderr, "Track gain, Track peak\n");
        }
        g_slist_foreach(files, (GFunc) print_file_data, NULL);
    }
    g_slist_foreach(files, (GFunc) destroy_state, NULL);
    scanner_reset_common();

    return do_scan;
}

int tag_files(GSList *files) {
    int ret = 0;

    fprintf(stderr, "Tagging");
    g_slist_foreach(files, (GFunc) tag_file, &ret);
    if (!ret) fprintf(stderr, " Success!");
    fputc('\n', stderr);

    return ret;
}

int loudness_tag(GSList *files)
{
    if (scan_files(files) && !dry_run) {
        return tag_files(files);
    }
    return 0;
}

gboolean loudness_tag_parse(int *argc, char **argv[])
{
    gboolean success = parse_mode_args(argc, argv, entries);
    if (!success) {
        if (*argc == 1) fprintf(stderr, "Missing arguments\n");
        return FALSE;
    }
    return TRUE;
}

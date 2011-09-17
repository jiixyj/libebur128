#ifdef G_OS_WIN32
#include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>

#include <glib.h>
#include <glib/gstdio.h>

#include "filetree.h"
#include "input.h"
#include "parse_global_args.h"

static void get_number_of_frames(gpointer user, gpointer user_data)
{
    struct filename_list_node *fln = (struct filename_list_node *) user;
    size_t *number_of_frames;

    struct input_ops* ops = NULL;
    struct input_handle* ih = NULL;
    FILE *file = NULL;
    int result;


    (void) user_data;
    fln->d = g_malloc(sizeof(size_t));
    number_of_frames = (size_t *) fln->d;
    *number_of_frames = 0;


    ops = input_get_ops(fln->fr->raw);
    if (!ops) {
        fprintf(stderr, "No plugin found for file '%s'\n", fln->fr->display);
        goto free;
    }
    ih = ops->handle_init();

    file = g_fopen(fln->fr->raw, "rb");
    if (!file) {
        fprintf(stderr, "Error opening file '%s'\n", fln->fr->display);
        goto free;
    }
    result = ops->open_file(ih, file, fln->fr->raw);
    if (result) {
        fprintf(stderr, "Error opening file '%s'\n", fln->fr->display);
        goto free;
    }

    *number_of_frames = ops->get_total_frames(ih);

  free:
    if (file) ops->close_file(ih, file);
    if (ih) ops->handle_destroy(&ih);
}

static void print_help(void) {
    printf("Usage: loudness scan|tag|dump [OPTION...] [FILE|DIRECTORY]...\n");
    printf("\n");
    printf("`loudness' scans audio files according to the EBU R128 standard. It can output\n");
    printf("loudness and peak information, write it to ReplayGain conformant tags, or dump\n");
    printf("momentary/shortterm/integrated loudness in fixed intervals to the console.\n");
    printf("\n");
    printf("Examples:\n");
    printf("  loudness scan foo.wav       # Scans foo.wav and writes information to stdout.\n");
    printf("  loudness tag -r bar/        # Tag all files in foo as one album per subfolder.\n");
    printf("  loudness dump -m 1.0 a.wav  # Each second, write momentary loudness to stdout.\n");
    printf("\n");
    printf(" Main operation mode:\n");
    printf("  scan                       output loudness and peak information\n");
    printf("  tag                        tag files with ReplayGain conformant tags\n");
    printf("  dump                       output momentary/shortterm/integrated loudness\n");
    printf("                             in fixed intervals\n");
    printf("\n");
    printf(" Global options:\n");
    printf("  -r, --recursive            recursively scan files in subdirectories\n");
    printf("  -L, --follow-symlinks      follow symbolic links (*nix only)\n");
    printf("  --no-sort                  do not sort command line arguments alphabetically\n");
    printf("  --force-plugin=PLUGIN      force input plugin; PLUGIN is one of:\n");
    printf("                             sndfile, mpg123, musepack, ffmpeg\n");
    printf("\n");
    printf(" Scan options:\n");
    printf("  -l, --lra                  calculate loudness range in LRA\n");
    printf("  -p, --peak=sample|true|dbtp|all  -p sample: sample peak (float value)\n");
    printf("                                   -p true:   true peak (float value)\n");
    printf("                                   -p dbtp:   true peak (dB True Peak)\n");
    printf("                                   -p all:    show all peak values\n");
    printf("\n");
    printf(" Tag options:\n");
    printf("  -t, --track                write only track gain (album gain is default)\n");
    printf("  -n, --dry-run              perform a trial run with no changes made\n");
    printf("\n");
    printf(" Dump options:\n");
    printf("  -m, --momentary=INTERVAL   print momentary loudness every INTERVAL seconds\n");
    printf("  -s, --shortterm=INTERVAL   print shortterm loudness every INTERVAL seconds\n");
    printf("  -i, --integrated=INTERVAL  print integrated loudness every INTERVAL seconds\n");
}

static gboolean recursive = FALSE;
static gboolean follow_symlinks = FALSE;
static gboolean no_sort = FALSE;
static gchar *forced_plugin = NULL;

static GOptionEntry entries[] =
{
    { "recursive", 'r', 0, G_OPTION_ARG_NONE, &recursive, NULL, NULL },
    { "follow-symlinks", 'L', 0, G_OPTION_ARG_NONE, &follow_symlinks, NULL, NULL },
    { "no-sort", 0, 0, G_OPTION_ARG_NONE, &no_sort, NULL, NULL },
    { "force-plugin", 0, 0, G_OPTION_ARG_STRING, &forced_plugin, NULL, NULL },
    { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, 0 }
};

int main(int argc, char *argv[])
{
    GSList *errors = NULL, *files = NULL;
    Filetree tree;

    if (parse_global_args(&argc, &argv, entries, TRUE)) {
        print_help();
        exit(EXIT_FAILURE);
    }
    g_thread_init(NULL);
    input_init(argv[0], forced_plugin);


    setlocale(LC_COLLATE, "");
    setlocale(LC_CTYPE, "");
    tree = filetree_init(&argv[1], (size_t) (argc - 1),
                         recursive, follow_symlinks, no_sort, &errors);
    /* filetree_print(tree); */

    g_slist_foreach(errors, filetree_print_error, NULL);
    g_slist_foreach(errors, filetree_free_error, NULL);
    g_slist_free(errors);

    filetree_file_list(tree, &files);
    g_slist_foreach(files, get_number_of_frames, NULL);
    g_slist_foreach(files, filetree_print_file_size, NULL);
    g_slist_foreach(files, filetree_free_list_entry, NULL);
    g_slist_free(files);

    filetree_destroy(tree);
    input_deinit();

    return EXIT_SUCCESS;
}

#include "scanner-tag.h"

#include "parse_args.h"
#include "scanner-common.h"
#include "nproc.h"

extern gboolean verbose;
static gboolean track = FALSE;
static gboolean dry_run = FALSE;

static GOptionEntry entries[] =
{
    { "track", 't', 0, G_OPTION_ARG_NONE, &track, NULL, NULL },
    { "dry-run", 'n', 0, G_OPTION_ARG_STRING, &dry_run, NULL, NULL },
    { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, 0 }
};

void loudness_tag(GSList *files)
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

    // g_slist_foreach(files, print_file_data, NULL);
    // print_summary(files);
    g_slist_foreach(files, destroy_state, NULL);
}

gboolean loudness_tag_parse(int *argc, char **argv[])
{
    return parse_mode_args(argc, argv, entries);
}

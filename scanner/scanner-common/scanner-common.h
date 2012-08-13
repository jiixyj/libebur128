#ifndef SCANNER_COMMON_H
#define SCANNER_COMMON_H

#include "ebur128.h"
#include "input.h"
#include "filetree.h"

#include <glib.h>

struct file_data
{
    ebur128_state *st;
    size_t number_of_frames;
    size_t number_of_elapsed_frames;
    double loudness;
    double lra;
    double peak;
    double true_peak;

    double gain_album;
    double peak_album;

    void *user;

    gboolean scanned;
    int tagged;
};

struct scan_opts
{
    gboolean lra;
    gchar *peak;
    gboolean histogram;

    /* used if in tag mode to force dual mono */
    gboolean force_dual_mono;
    /* if non-zero, decode all input audio to this file */
    gchar *decode_file;
};

extern GMutex *progress_mutex;
extern GCond *progress_cond;
extern guint64 elapsed_frames;
extern guint64 total_frames;

int open_plugin(const char *raw, const char *display,
                struct input_ops **ops,
                struct input_handle **ih);
void scanner_init_common(void);
void scanner_reset_common(void);
void init_and_get_number_of_frames(struct filename_list_node *fln, int *do_scan);
void init_state_and_scan_work_item(struct filename_list_node *fln, struct scan_opts *opts);
void init_state_and_scan(gpointer work_item, GThreadPool *pool);
void destroy_state(struct filename_list_node *fln, gpointer unused);
void get_state(struct filename_list_node *fln, GPtrArray *states);
void get_max_peaks(struct filename_list_node *fln, struct file_data *result);
void clear_line(void);
void process_files(GSList *files, struct scan_opts *opts);

#endif /* end of include guard: SCANNER_COMMON_H */

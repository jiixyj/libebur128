#ifndef SCANNER_SCAN_H
#define SCANNER_SCAN_H

#include "ebur128.h"

#include <glib.h>

struct file_data
{
    ebur128_state *st;
    size_t number_of_frames;
    size_t number_of_elapsed_frames;
    GMutex *mutex;
    double loudness;
};

void loudness_scan(GSList *files);
gboolean loudness_scan_parse(int *argc, char **argv[]);

#endif /* end of include guard: SCANNER_SCAN_H */

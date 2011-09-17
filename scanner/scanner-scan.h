#ifndef SCANNER_SCAN_H
#define SCANNER_SCAN_H

#include "ebur128.h"

#include <glib.h>

struct file_data
{
    ebur128_state *st;
    size_t number_of_frames;
    double loudness;
};

void init_and_get_number_of_frames(gpointer user, gpointer user_data);
void init_state_and_scan(gpointer user, gpointer user_data);
void print_file_data(gpointer user, gpointer user_data);
void destroy_state(gpointer user, gpointer user_data);


#endif /* end of include guard: SCANNER_SCAN_H */

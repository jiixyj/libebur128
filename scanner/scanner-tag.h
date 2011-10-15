#ifndef SCANNER_TAG_H
#define SCANNER_TAG_H

#include <glib.h>

#include "filetree.h"

#define RG_REFERENCE_LEVEL -18.0
double clamp_rg(double x);

void tag_file(struct filename_list_node *fln, int *ret);
int scan_files(GSList *files);
int tag_files(GSList *files);
int loudness_tag(GSList *files);
gboolean loudness_tag_parse(int *argc, char **argv[]);

#endif /* end of include guard: SCANNER_TAG_H */

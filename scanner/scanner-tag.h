#ifndef SCANNER_TAG_H
#define SCANNER_TAG_H

#include <glib.h>

void loudness_tag(GSList *files);
gboolean loudness_tag_parse(int *argc, char **argv[]);

#endif /* end of include guard: SCANNER_TAG_H */

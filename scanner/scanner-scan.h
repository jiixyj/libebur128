#ifndef SCANNER_SCAN_H
#define SCANNER_SCAN_H

#include <glib.h>

void loudness_scan(GSList *files);
gboolean loudness_scan_parse(int *argc, char **argv[]);

#endif /* end of include guard: SCANNER_SCAN_H */

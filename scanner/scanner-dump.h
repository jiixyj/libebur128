#ifndef SCANNER_DUMP_H
#define SCANNER_DUMP_H

#include <glib.h>

int loudness_dump(GSList *files);
gboolean loudness_dump_parse(int *argc, char **argv[]);

#endif /* end of include guard: SCANNER_DUMP_H */

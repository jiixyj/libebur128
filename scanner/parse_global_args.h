#ifndef PARSE_GLOBAL_ARGS_H
#define PARSE_GLOBAL_ARGS_H

#include <glib.h>

int parse_global_args(int *argc, char ***argv,
                      GOptionEntry *entries,
                      gboolean ignore_unknown);
gboolean parse_mode_args(int *argc, char **argv[],
                         GOptionEntry *entries);

#endif /* end of include guard: PARSE_GLOBAL_ARGS_H */

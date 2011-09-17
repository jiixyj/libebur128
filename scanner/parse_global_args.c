#include "parse_global_args.h"

#ifdef G_OS_WIN32
extern int __wgetmainargs(int *_Argc, wchar_t ***_Argv, wchar_t ***_Env,
                          int _DoWildCard, STARTUPINFO *_StartInfo);
#endif

int parse_global_args(int *argc, char ***argv,
                      GOptionEntry *entries,
                      gboolean ignore_unknown)
{
    GError *error = NULL;
    GOptionContext *context = g_option_context_new(NULL);
#ifdef G_OS_WIN32
    wchar_t **wargv, **wenv;
    STARTUPINFO si = {0};
    int i;

    __wgetmainargs(argc, &wargv, &wenv, TRUE, &si);
    *argv = g_new(gchar *, *argc + 1);
    for (i = 0; i < *argc; ++i) {
        (*argv)[i] = g_utf16_to_utf8(wargv[i], -1, NULL, NULL, NULL);
    }
    (*argv)[i] = NULL;
#endif

    g_option_context_add_main_entries(context, entries, NULL);
    g_option_context_set_ignore_unknown_options(context, ignore_unknown);
    g_option_context_set_help_enabled(context, FALSE);
    if (!g_option_context_parse(context, argc, argv, &error)) {
        g_print("%s\n", error->message);
        g_option_context_free(context);
        return 1;
    }
    if (*argc == 1) {
        g_option_context_free(context);
        return 1;
    }
    g_option_context_free(context);
    return 0;
}

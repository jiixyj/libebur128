/* See LICENSE file for copyright and license details. */
#include "input.h"

#include <gmodule.h>
#include <stdio.h>

#ifdef HAVE_CONFIG_PLUGIN_PATH_H
#include "plugin_path.h"
#else
#define PLUGIN_PATH ""
#endif

static const char* plugin_names[] = {
  "input_sndfile",
  "input_mpg123",
  "input_musepack",
  "input_ffmpeg",
#ifndef GSTREAMER_INPUT_STATIC
  "input_gstreamer",
#endif
  NULL
};

static const char* plugin_search_dirs[] = {
  ".",
  "r128",
  "",
  NULL, /* = g_path_get_dirname(av0); */
  PLUGIN_PATH,
  NULL
};

static GSList* g_modules;
static GSList* plugin_ops; /*struct input_ops* ops;*/
static GSList* plugin_exts;

extern int verbose;
static int plugin_forced;

static void search_module_in_paths(const char* plugin,
                                   GModule** module,
                                   const char* const* search_dir) {
  int search_dir_index = 0;
  while (!*module && search_dir[search_dir_index]) {
    char* path = g_module_build_path(search_dir[search_dir_index], plugin);
    *module = g_module_open(path, G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);
    g_free(path);
    ++search_dir_index;
  }
}

int input_init(char* exe_name, const char* forced_plugin) {
  int plugin_found = 0;
  const char** cur_plugin_name = plugin_names;
  struct input_ops* ops;
  char** exts;
  GModule* module;
  char* exe_dir;
  const char* env_path;
  char** env_path_split;
  char** it;

  exe_dir = g_path_get_dirname(exe_name);
  plugin_search_dirs[3] = exe_dir;

  env_path = g_getenv("PATH");
  env_path_split = g_strsplit(env_path, G_SEARCHPATH_SEPARATOR_S, 0);
  for (it = env_path_split; *it; ++it) {
    char* r128_path = g_build_filename(*it, "r128", NULL);
    g_free(*it);
    *it = r128_path;
  }

  if (forced_plugin) plugin_forced = 1;
  /* Load plugins */
  while (*cur_plugin_name) {
    if (forced_plugin && strcmp(forced_plugin, (*cur_plugin_name) + 6)) {
      ++cur_plugin_name;
      continue;
    }
    ops = NULL;
    exts = NULL;
    module = NULL;
    search_module_in_paths(*cur_plugin_name, &module, plugin_search_dirs);
    search_module_in_paths(*cur_plugin_name, &module,
                           (const char* const*) env_path_split);
    if (!module) {
      /* fprintf(stderr, "%s\n", g_module_error()); */
    } else {
      if (!g_module_symbol(module, "ip_ops", (gpointer*) &ops)) {
        fprintf(stderr, "%s: %s\n", *cur_plugin_name, g_module_error());
      }
      if (!g_module_symbol(module, "ip_exts", (gpointer*) &exts)) {
        fprintf(stderr, "%s: %s\n", *cur_plugin_name, g_module_error());
      }
    }
    if (ops) {
      if (verbose) fprintf(stderr, "found plugin %s\n", *cur_plugin_name);
      ops->init_library();
      plugin_found = 1;
    }
    g_modules = g_slist_append(g_modules, module);
    plugin_ops = g_slist_append(plugin_ops, ops);
    plugin_exts = g_slist_append(plugin_exts, exts);
    ++cur_plugin_name;
  }
#ifdef GSTREAMER_INPUT_STATIC
  plugin_ops = g_slist_append(plugin_ops, &gstreamer_ip_ops);
  plugin_exts = g_slist_append(plugin_exts, &gstreamer_ip_exts);
  gstreamer_ip_ops.init_library();
  plugin_found = 1;
#endif
  g_free(exe_dir);
  g_strfreev(env_path_split);
  if (!plugin_found) {
    fprintf(stderr, "Warning: no plugins found!\n");
    return 1;
  }
  return 0;
}

int input_deinit(void) {
  /* unload plugins */
  GSList* ops = plugin_ops;
  GSList* modules = g_modules;
#ifdef GSTREAMER_INPUT_STATIC
  gstreamer_ip_ops.exit_library();
#endif
  while (ops && modules) {
    if (ops->data && modules->data) {
      ((struct input_ops*) ops->data)->exit_library();
      if (!g_module_close((GModule*) modules->data)) {
        fprintf(stderr, "%s\n", g_module_error());
      }
    }
    ops = g_slist_next(ops);
    modules = g_slist_next(modules);
  }
  g_slist_free(g_modules);
  g_slist_free(plugin_ops);
  g_slist_free(plugin_exts);
  return 0;
}

struct input_ops* input_get_ops(const char* filename) {
  static char empty[] = { '\0' };
  GSList* ops = plugin_ops;
  GSList* exts = plugin_exts;
  char* filename_ext = strrchr(filename, '.');

  if (filename_ext) {
    ++filename_ext;
  } else {
    filename_ext = &empty[0];
  }
  while (ops && exts) {
    if (ops->data && exts->data) {
      const char** cur_exts = exts->data;
      if (!(*cur_exts)) {
        return (struct input_ops*) ops->data;
      }
      while (*cur_exts) {
        if (!g_ascii_strcasecmp(filename_ext, *cur_exts) || plugin_forced) {
          return (struct input_ops*) ops->data;
        }
        ++cur_exts;
      }
    }
    ops = g_slist_next(ops);
    exts = g_slist_next(exts);
  }
  return NULL;
}

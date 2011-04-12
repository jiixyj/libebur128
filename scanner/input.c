/* See LICENSE file for copyright and license details. */
#include "input.h"

#include <gmodule.h>

static const char* plugin_names[] = {"input_sndfile",
                                     "input_mpg123",
                                     "input_musepack",
                                     "input_ffmpeg",
                                     NULL};
static GSList* g_modules = NULL;
static GSList* plugin_ops = NULL; /*struct input_ops* ops;*/
static GSList* plugin_exts = NULL;

int input_init() {
  /* Load plugins */
  int plugin_found = 0;
  const char** cur_plugin_name = plugin_names;
  while (*cur_plugin_name) {
    struct input_ops* ops = NULL;
    char** exts = NULL;
    GModule* module = g_module_open(*cur_plugin_name,
                            G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);
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
      ops->init_library();
      plugin_found = 1;
    }
    g_modules = g_slist_append(g_modules, module);
    plugin_ops = g_slist_append(plugin_ops, ops);
    plugin_exts = g_slist_append(plugin_exts, exts);
    ++cur_plugin_name;
  }
  if (!plugin_found) {
    fprintf(stderr, "Warning: no plugins found!\n");
    return 1;
  }
  return 0;
}

int input_deinit() {
  /* unload plugins */
  GSList* ops = plugin_ops;
  GSList* modules = g_modules;
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
  GSList* ops = plugin_ops;
  GSList* exts = plugin_exts;
  while (ops && exts) {
    if (ops->data && exts->data) {
      const char** cur_exts = exts->data;
      char* filename_ext = strrchr(filename, '.');
      if (filename_ext) ++filename_ext;
      while (*cur_exts) {
        if (!strcmp(filename_ext, *cur_exts)) {
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

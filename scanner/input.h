/* See LICENSE file for copyright and license details. */
#ifndef _INPUT_H_
#define _INPUT_H_

#include <string.h>
#include <stdio.h>

#include <gmodule.h>

#include "ebur128.h"

struct input_handle;

struct input_ops {
  unsigned      (*get_channels)(struct input_handle* ih);
  unsigned long (*get_samplerate)(struct input_handle* ih);
  float*        (*get_buffer)(struct input_handle* ih);
  size_t        (*get_buffer_size)(struct input_handle* ih);
  struct input_handle* (*handle_init)();
  void          (*handle_destroy)(struct input_handle** ih);
  int           (*open_file)(struct input_handle* ih, FILE* file);
  int           (*set_channel_map)(struct input_handle* ih, ebur128_state* st);
  int           (*allocate_buffer)(struct input_handle* ih);
  size_t        (*read_frames)(struct input_handle* ih);
  int           (*check_ok)(struct input_handle* ih, size_t nr_frames_read_all);
  void          (*free_buffer)(struct input_handle* ih);
  void          (*close_file)(struct input_handle* ih, FILE* file);
  int           (*init_library)();
  void          (*exit_library)();
};

int input_init(const char* forced_plugin);
int input_deinit(void);
struct input_ops* input_get_ops(const char* filename);

#endif  /* _INPUT_H_ */

/* See LICENSE file for copyright and license details. */
#ifndef _INPUT_H_
#define _INPUT_H_

#include <string.h>

struct input_handle;

struct input_ops {
  unsigned      (*get_channels)(struct input_handle* ih);
  unsigned long (*get_samplerate)(struct input_handle* ih);
  float*        (*get_buffer)(struct input_handle* ih);
  struct input_handle* (*handle_init)();
  void          (*handle_destroy)(struct input_handle** ih);
  int           (*open_file)(struct input_handle* ih, const char* filename);
  int           (*set_channel_map)(struct input_handle* ih, int* st);
  int           (*allocate_buffer)(struct input_handle* ih);
  size_t        (*get_total_frames)(struct input_handle* ih);
  size_t        (*read_frames)(struct input_handle* ih);
  void          (*free_buffer)(struct input_handle* ih);
  void          (*close_file)(struct input_handle* ih);
  int           (*init_library)();
  void          (*exit_library)();
};

int input_init(char* exe_name, const char* forced_plugin);
int input_deinit(void);
struct input_ops* input_get_ops(const char* filename);

#ifdef GSTREAMER_INPUT_STATIC
struct input_ops gstreamer_ip_ops;
const char *gstreamer_ip_exts[1];
#endif

int input_open_fd(const char* filename);
void input_close_fd(int fd);
int input_read_fd(int fd, void *buf, unsigned int count);

#endif  /* _INPUT_H_ */

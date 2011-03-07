/* See LICENSE file for copyright and license details. */
#ifndef _INPUT_H_
#define _INPUT_H_

struct input_handle;
size_t input_get_channels(struct input_handle* ih);
size_t input_get_samplerate(struct input_handle* ih);
float* input_get_buffer(struct input_handle* ih);
size_t input_get_buffer_size(struct input_handle* ih);
struct input_handle* input_handle_init();
void input_handle_destroy(struct input_handle** ih);
int input_open_file(struct input_handle* ih, FILE* file);
int input_set_channel_map(struct input_handle* ih, ebur128_state* st);
int input_allocate_buffer(struct input_handle* ih);
size_t input_read_frames(struct input_handle* ih);
int input_check_ok(struct input_handle* ih, size_t nr_frames_read_all);
void input_free_buffer(struct input_handle* ih);
void input_close_file(struct input_handle* ih, FILE* file);
int input_init_library();
void input_exit_library();

#endif  /* _INPUT_H_ */

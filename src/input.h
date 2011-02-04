/* See LICENSE file for copyright and license details. */
#ifndef _INPUT_H_
#define _INPUT_H_

extern struct input_handle;
extern int input_get_channels(struct input_handle* ih);
extern int input_get_samplerate(struct input_handle* ih);
extern float* input_get_buffer(struct input_handle* ih);
extern struct input_handle* input_handle_init();
extern void input_handle_destroy(struct input_handle** ih);
extern int input_open_file(struct input_handle* ih, const char* filename);
extern int input_set_channel_map(struct input_handle* ih, ebur128_state* st);
extern int input_allocate_buffer(struct input_handle* ih);
extern size_t input_read_frames(struct input_handle* ih);
extern int input_check_ok(struct input_handle* ih, size_t nr_frames_read_all);
extern void input_free_buffer(struct input_handle* ih);
extern void input_close_file(struct input_handle* ih);
extern int input_init_library();
extern void input_exit_library();

#endif  /* _INPUT_H_ */

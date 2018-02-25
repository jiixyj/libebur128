#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>
#include <unistd.h>

#include <string>

#include "ebur128.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  unsigned int channels;
  unsigned long samplerate;
  int mode;
  unsigned int new_channels;
  unsigned long new_samplerate;
  unsigned long window;

  size_t data_offset = sizeof(channels) + sizeof(samplerate) + sizeof(mode) +
                       sizeof(new_channels) + sizeof(new_samplerate) +
                       sizeof(window);

  if (size < data_offset) {
    return 0;
  }

  memcpy(&channels, data, sizeof(channels));
  data += sizeof(channels);
  memcpy(&samplerate, data, sizeof(samplerate));
  data += sizeof(samplerate);
  memcpy(&mode, data, sizeof(mode));
  data += sizeof(mode);
  memcpy(&new_channels, data, sizeof(new_channels));
  data += sizeof(new_channels);
  memcpy(&new_samplerate, data, sizeof(new_samplerate));
  data += sizeof(new_samplerate);
  memcpy(&window, data, sizeof(window));
  data += sizeof(window);

  if (new_channels < channels) {
    return 0;
  }

  size -= data_offset;

  ebur128_state* state = ebur128_init(channels, samplerate, mode);

  if (state) {
    ebur128_add_frames_int(state, (int const*) data,
                           size / channels / sizeof(int));
    ebur128_set_max_window(state, window);
    ebur128_change_parameters(state, new_channels, new_samplerate);
    ebur128_add_frames_int(state, (int const*) data,
                           size / new_channels / sizeof(int));

    ebur128_destroy(&state);
  }
  return 0;
}

#include <ebur128.h>

int main(void) {
  ebur128_state* state = ebur128_init(2, 48000, EBUR128_MODE_I);
  if (!state) {
    return 1;
  }
  ebur128_destroy(&state);
  return 0;
}

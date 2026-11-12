#undef NDEBUG
#include <cassert>

#include <ebur128.h>

namespace ebur128 {

class state {
public:
  explicit state(unsigned int channels, unsigned long samplerate, int mode)
      : state_(ebur128_init(channels, samplerate, mode)) {}
  state(const state&) = delete;
  state& operator=(const state&) = delete;
  ~state() { ebur128_destroy(&state_); }

private:
  ebur128_state* state_;
};

} // namespace ebur128

static bool test_ebur128_simple() {
  ebur128::state s(2, 44100, EBUR128_MODE_I);
  (void) s;
  return true;
}

int main() {
  //
  assert(test_ebur128_simple());
}

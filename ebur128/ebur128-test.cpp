#undef NDEBUG
#include <cassert>

#include <atomic>
#include <stdexcept>
#include <thread>
#include <vector>

#include <ebur128.h>

namespace ebur128 {

class error : public std::runtime_error {
  using std::runtime_error::runtime_error;
};

class state {
public:
  explicit state(unsigned int channels, unsigned long samplerate, int mode)
      : state_(ebur128_init(channels, samplerate, mode)) {
    if (!state_)
      throw ebur128::error("ebur128_init");
  }
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

static bool test_ebur128_threaded_init() {
  const unsigned thread_count = 8;
  std::atomic<unsigned> ready = 0;
  std::atomic<bool> start = false;
  std::vector<std::thread> threads = {};
  threads.reserve(thread_count);

  for (unsigned i = 0; i < thread_count; ++i) {
    threads.emplace_back([&] {
      ready.fetch_add(1, std::memory_order_release);
      while (!start.load(std::memory_order_acquire)) {
      }

      ebur128::state s(1, 48000, EBUR128_MODE_I);
      (void) s;
    });
  }

  while (ready.load(std::memory_order_acquire) < thread_count) {
  }
  start.store(true, std::memory_order_release);

  for (auto& thread : threads) {
    thread.join();
  }
  return true;
}

int main() {
  assert(test_ebur128_simple());
  assert(test_ebur128_threaded_init());
}

#undef NDEBUG
#include <cassert>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <limits>
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

  void add_frames_double(const double* src, std::size_t frames) {
    if (ebur128_add_frames_double(state_, src, frames) != EBUR128_SUCCESS) {
      throw ebur128::error("ebur128_add_frames_double");
    }
  }

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

#ifdef EBUR128_TEST_PERFORMANCE

namespace {

constexpr unsigned int ftz_samplerate = 48000;
constexpr std::size_t ftz_seconds = 10;
constexpr std::size_t ftz_frames = ftz_samplerate * ftz_seconds;
constexpr std::size_t ftz_block_frames = ftz_samplerate / 10;
constexpr int ftz_repetitions = 7;
constexpr double ftz_max_ratio = 1.5;

std::vector<double> make_ftz_input(double impulse) {
  std::vector<double> input(ftz_frames, 0.0);
  for (std::size_t i = 0; i < ftz_frames; i += ftz_block_frames) {
    input[i] = impulse;
  }
  return input;
}

double measure_ftz(const std::vector<double>& input) {
  ebur128::state s(1, ftz_samplerate, EBUR128_MODE_M);
  const auto start = std::chrono::steady_clock::now();
  s.add_frames_double(input.data(), input.size());
  const auto end = std::chrono::steady_clock::now();
  return std::chrono::duration<double>(end - start).count();
}

bool test_ebur128_ftz_performance() {
  const std::vector<double> denormal_input =
      make_ftz_input(std::numeric_limits<double>::min() * 8.0);
  const std::vector<double> normal_input = make_ftz_input(1e-3);

  (void) measure_ftz(denormal_input);
  (void) measure_ftz(normal_input);

  double denormal_time = std::numeric_limits<double>::max();
  double normal_time = std::numeric_limits<double>::max();
  for (int i = 0; i < ftz_repetitions; ++i) {
    if (i % 2 == 0) {
      denormal_time = std::min(denormal_time, measure_ftz(denormal_input));
      normal_time = std::min(normal_time, measure_ftz(normal_input));
    } else {
      normal_time = std::min(normal_time, measure_ftz(normal_input));
      denormal_time = std::min(denormal_time, measure_ftz(denormal_input));
    }
  }

  const double ratio = denormal_time / std::max(normal_time, 1e-9);
  if (ratio >= ftz_max_ratio) {
    std::fprintf(stderr,
                 "FTZ performance test failed: denormal %.6fs, normal %.6fs, "
                 "ratio %.2f\n",
                 denormal_time, normal_time, ratio);
    return false;
  }
  return true;
}

} // namespace

#endif

int main() {
  assert(test_ebur128_simple());
  assert(test_ebur128_threaded_init());
#ifdef EBUR128_TEST_PERFORMANCE
  assert(test_ebur128_ftz_performance());
#endif
}

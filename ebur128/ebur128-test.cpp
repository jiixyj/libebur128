#undef NDEBUG
#include <cassert>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <span>
#include <stdexcept>
#include <thread>
#include <vector>

#include <ebur128.h>

namespace ebur128 {

// clang-format off
class error : public std::runtime_error { using std::runtime_error::runtime_error; };
class nomem_error                 : public error { using ebur128::error::error; };
class invalid_mode_error          : public error { using ebur128::error::error; };
class invalid_channel_index_error : public error { using ebur128::error::error; };
class no_change_error             : public error { using ebur128::error::error; };
// clang-format on

//

enum class channel : unsigned int {};
enum class samplerate : unsigned long {};
enum class mode : int {
  m = EBUR128_MODE_M,
  s = EBUR128_MODE_S,
  i = EBUR128_MODE_I,
  lra = EBUR128_MODE_LRA,
  sample_peak = EBUR128_MODE_SAMPLE_PEAK,
  true_peak = EBUR128_MODE_TRUE_PEAK,
  histogram = EBUR128_MODE_HISTOGRAM,
};

template <class T>
constexpr std::underlying_type_t<T> to_underlying(T val) noexcept {
  return static_cast<std::underlying_type_t<T>>(val);
}

ebur128::mode operator|(ebur128::mode lhs, ebur128::mode rhs) {
  return ebur128::mode{ebur128::to_underlying(lhs) |
                       ebur128::to_underlying(rhs)};
}

//

template <class T>
concept sample_type = std::same_as<T, short> || std::same_as<T, int> ||
                      std::same_as<T, float> || std::same_as<T, double>;

//

template <sample_type T> class audio_data_view {
public:
  explicit audio_data_view(std::span<const T> data, channel nr_channels)
      : data_(data), channels_(nr_channels) {
    if (data_.size() % ebur128::to_underlying(channels_))
      throw std::logic_error("audio_data_view");
  }

  std::span<const T> data() const { return data_; }
  std::size_t frames() const {
    return data_.size() / ebur128::to_underlying(channels_);
  }

private:
  std::span<const T> data_;
  ebur128::channel channels_;
};

template <std::ranges::contiguous_range R>
audio_data_view(R&&, ebur128::channel)
    -> audio_data_view<std::remove_const_t<std::ranges::range_value_t<R>>>;

//

class state {
public:
  explicit state(ebur128::channel channels,
                 ebur128::samplerate samplerate,
                 ebur128::mode mode)
      : state_(ebur128_init(ebur128::to_underlying(channels),
                            ebur128::to_underlying(samplerate),
                            ebur128::to_underlying(mode))) {
    if (!state_)
      throw ebur128::error("ebur128_init");
  }
  state(const state&) = delete;
  state& operator=(const state&) = delete;
  ~state() { ebur128_destroy(&state_); }

  template <sample_type T> void add_frames(audio_data_view<T> data);

  void set_channel(ebur128::channel channel_index, ::channel channel) {
    state::handle_error(
        [&] {
          return ebur128_set_channel(
              state_, ebur128::to_underlying(channel_index), channel);
        },
        "ebur128_set_channel");
  }

  void set_max_window(std::chrono::milliseconds window) {
    state::handle_error(
        [&] {
          return ebur128_set_max_window(
              state_, static_cast<unsigned long>(window.count()));
        },
        "ebur128_set_max_window");
  }

  double loudness_window(std::chrono::milliseconds window) {
    double out;
    state::handle_error(
        [&] {
          return ebur128_loudness_window(
              state_, static_cast<unsigned long>(window.count()), &out);
        },
        "ebur128_loudness_window");
    return out;
  }

private:
  template <class F> static void handle_error(F f, const char* msg) {
    switch (static_cast<enum ::error>(f())) {
      case EBUR128_SUCCESS: //
        return;
      case EBUR128_ERROR_NOMEM: //
        throw ebur128::nomem_error(msg);
      case EBUR128_ERROR_INVALID_MODE: //
        throw ebur128::invalid_mode_error(msg);
      case EBUR128_ERROR_INVALID_CHANNEL_INDEX:
        throw ebur128::invalid_channel_index_error(msg);
      case EBUR128_ERROR_NO_CHANGE: //
        throw ebur128::no_change_error(msg);
    }

    throw ebur128::error(msg);
  }

  ebur128_state* state_;
};

template <> inline void state::add_frames<short>(audio_data_view<short> data) {
  state::handle_error(
      [&] {
        return ebur128_add_frames_short(state_, data.data().data(),
                                        data.frames());
      },
      "ebur128_add_frames_short");
}

template <> inline void state::add_frames<int>(audio_data_view<int> data) {
  state::handle_error(
      [&] {
        return ebur128_add_frames_int(state_, data.data().data(),
                                      data.frames());
      },
      "ebur128_add_frames_int");
}

template <> inline void state::add_frames<float>(audio_data_view<float> data) {
  state::handle_error(
      [&] {
        return ebur128_add_frames_float(state_, data.data().data(),
                                        data.frames());
      },
      "ebur128_add_frames_float");
}

template <>
inline void state::add_frames<double>(audio_data_view<double> data) {
  state::handle_error(
      [&] {
        return ebur128_add_frames_double(state_, data.data().data(),
                                         data.frames());
      },
      "ebur128_add_frames_double");
}

} // namespace ebur128

static bool test_ebur128_simple() {
  ebur128::state s(ebur128::channel{2}, ebur128::samplerate{44100},
                   ebur128::mode::i);
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

      ebur128::state s(ebur128::channel{1}, ebur128::samplerate{48000},
                       ebur128::mode::i);
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
constexpr std::size_t ftz_seconds = 20;
constexpr std::size_t ftz_frames = ftz_samplerate * ftz_seconds;
constexpr std::size_t ftz_block_frames = ftz_samplerate * 2 / 5;
constexpr int ftz_repetitions = 31;
constexpr double ftz_max_ratio = 1.05;

std::vector<double> make_ftz_input(double impulse) {
  std::vector<double> input(ftz_frames, 0.0);
  for (std::size_t i = 0; i < ftz_frames; i += ftz_block_frames) {
    input[i] = impulse;
  }
  return input;
}

double measure_ftz(const std::vector<double>& input) {
  ebur128::state s(ebur128::channel{1}, ebur128::samplerate{ftz_samplerate},
                   ebur128::mode::m);
  const auto start = std::chrono::steady_clock::now();
  s.add_frames(ebur128::audio_data_view(input, ebur128::channel{1}));
  const auto end = std::chrono::steady_clock::now();
  return std::chrono::duration<double>(end - start).count();
}

bool test_ebur128_ftz_performance() {
  const std::vector<double> denormal_input =
      make_ftz_input(std::numeric_limits<double>::min());
  const std::vector<double> normal_input = make_ftz_input(1e-3);

  (void) measure_ftz(denormal_input);
  (void) measure_ftz(normal_input);

  std::vector<double> ratios;
  ratios.reserve(ftz_repetitions);
  for (int i = 0; i < ftz_repetitions; ++i) {
    double denormal_time;
    double normal_time;
    if (i % 2 == 0) {
      denormal_time = measure_ftz(denormal_input);
      normal_time = measure_ftz(normal_input);
    } else {
      normal_time = measure_ftz(normal_input);
      denormal_time = measure_ftz(denormal_input);
    }
    ratios.push_back(denormal_time / std::max(normal_time, 1e-9));
  }

  std::sort(ratios.begin(), ratios.end());
  const double ratio = ratios[ratios.size() / 2];
  if (ratio >= ftz_max_ratio) {
    std::fprintf(stderr,
                 "FTZ performance test failed: ratio %.3f (min %.3f, max "
                 "%.3f)\n",
                 ratio, ratios.front(), ratios.back());
    return false;
  }
  return true;
}

} // namespace

#endif

static bool test_ebur128_init() {
  {
    (void) ebur128::state(ebur128::channel{1}, ebur128::samplerate{48000},
                          ebur128::mode::m);
    (void) ebur128::state(ebur128::channel{1}, ebur128::samplerate{48000},
                          ebur128::mode::s);
    (void) ebur128::state(ebur128::channel{1}, ebur128::samplerate{48000},
                          ebur128::mode::i);
  }

  {
    try {
      (void) ebur128::state(ebur128::channel{1}, ebur128::samplerate{48000},
                            ebur128::mode{});
      assert(false);
    } catch (const ebur128::error&) {
    }
  }

  {
    try {
      (void) ebur128::state(ebur128::channel{1}, ebur128::samplerate{48000},
                            ebur128::mode::histogram);
      assert(false);
    } catch (const ebur128::error&) {
    }
  }

  {
    try {
      (void) ebur128::state(ebur128::channel{0}, ebur128::samplerate{48000},
                            ebur128::mode::m);
      assert(false);
    } catch (const ebur128::error&) {
    }
  }

  {
    (void) ebur128::state(ebur128::channel{64}, ebur128::samplerate{48000},
                          ebur128::mode::m);
    try {
      (void) ebur128::state(ebur128::channel{65}, ebur128::samplerate{48000},
                            ebur128::mode::m);
      assert(false);
    } catch (const ebur128::error&) {
    }
  }

  {
    (void) ebur128::state(ebur128::channel{1}, ebur128::samplerate{16},
                          ebur128::mode::m);
    try {
      (void) ebur128::state(ebur128::channel{1}, ebur128::samplerate{15},
                            ebur128::mode::m);
      assert(false);
    } catch (const ebur128::error&) {
    }
  }

  {
    (void) ebur128::state(ebur128::channel{1}, ebur128::samplerate{2822400},
                          ebur128::mode::m);
    try {
      (void) ebur128::state(ebur128::channel{1}, ebur128::samplerate{2822401},
                            ebur128::mode::m);
      assert(false);
    } catch (const ebur128::error&) {
    }
  }

  return true;
}

static bool test_ebur128_set_channel() {
  {
    ebur128::state s(ebur128::channel{1}, ebur128::samplerate{48000},
                     ebur128::mode::m);

    s.set_channel(ebur128::channel{0}, EBUR128_DUAL_MONO);
    s.set_channel(ebur128::channel{0}, EBUR128_CENTER);

    try {
      s.set_channel(ebur128::channel{1}, EBUR128_CENTER);
      assert(false);
    } catch (const ebur128::invalid_channel_index_error&) {
    }
  }

  {
    ebur128::state s(ebur128::channel{2}, ebur128::samplerate{48000},
                     ebur128::mode::m);

    try {
      s.set_channel(ebur128::channel{0}, EBUR128_DUAL_MONO);
      assert(false);
    } catch (const ebur128::invalid_channel_index_error&) {
    }
  }

  return true;
}

// This tests for a (fixed) unsigned integer overflow in
// `ebur128_loudness_window`.
static bool test_ebur128_loudness_window_overflow() {
  ebur128::state s(ebur128::channel{1}, ebur128::samplerate{48000},
                   ebur128::mode::m);

  s.set_max_window(std::chrono::milliseconds{89479});

  constexpr double pi = 3.14159265358979323846;
  std::vector<short> input(19200);
  for (std::size_t i = 0; i < input.size(); ++i) {
    input[i] = static_cast<short>(
        32767.0 *
        std::sin(2.0 * pi * 1000.0 * static_cast<double>(i) / 48000.0));
  }
  s.add_frames(ebur128::audio_data_view(input, ebur128::channel{1}));

  const double long_window =
      s.loudness_window(std::chrono::milliseconds{89479});
  const double short_window = s.loudness_window(std::chrono::milliseconds{24});

  if (!(long_window < short_window - 10.0)) {
    std::fprintf(stderr,
                 "loudness_window overflow test failed: long=%.3f short=%.3f\n",
                 long_window, short_window);
    return false;
  }
  return true;
}

// 48 kHz mono, 2,796,200 ms allocates just under 1 GiB.
static bool test_ebur128_set_max_window_large_allocation() {
  ebur128::state s(ebur128::channel{1}, ebur128::samplerate{48000},
                   ebur128::mode::m);
  s.set_max_window(std::chrono::milliseconds{2'796'200});
  return true;
}

int main() {
  assert(test_ebur128_simple());
  assert(test_ebur128_threaded_init());
#ifdef EBUR128_TEST_PERFORMANCE
  assert(test_ebur128_ftz_performance());
#endif
  assert(test_ebur128_init());
  assert(test_ebur128_set_channel());
  assert(test_ebur128_loudness_window_overflow());
  assert(test_ebur128_set_max_window_large_allocation());
}

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
#include <ebur128.test_signals.h>

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

  double loudness_window(std::chrono::milliseconds window) const {
    double out;
    state::handle_error(
        [&] {
          return ebur128_loudness_window(
              state_, static_cast<unsigned long>(window.count()), &out);
        },
        "ebur128_loudness_window");
    return out;
  }

  double loudness_global() const {
    double out;
    state::handle_error([&] { return ebur128_loudness_global(state_, &out); },
                        "ebur128_loudness_global");
    return out;
  }

  double loudness_range() const {
    double out;
    state::handle_error([&] { return ebur128_loudness_range(state_, &out); },
                        "ebur128_loudness_range");
    return out;
  }

  double loudness_shortterm() const {
    double out;
    state::handle_error(
        [&] { return ebur128_loudness_shortterm(state_, &out); },
        "ebur128_loudness_shortterm");
    return out;
  }

  double loudness_momentary() const {
    double out;
    state::handle_error(
        [&] { return ebur128_loudness_momentary(state_, &out); },
        "ebur128_loudness_momentary");
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

namespace {

using test_signal = ebur128::test_signals::test_signal;

constexpr double test_signal_tolerance = 0.1;
constexpr double test_lra_tolerance = 1.0;
constexpr std::size_t test_signal_block_frames =
    static_cast<std::size_t>(
        ebur128::test_signals::test_signal::default_samplerate) /
    100;

void add_test_signal_frames(ebur128::state& s,
                            const ebur128::test_signals::test_signal& signal,
                            std::size_t first_frame,
                            std::size_t frames) {
  const auto signal_channels = signal.channels();
  const auto channels = static_cast<std::size_t>(signal_channels);
  const auto samples = signal.samples();
  s.add_frames(ebur128::audio_data_view<double>(
      samples.subspan(first_frame * channels, frames * channels),
      ebur128::channel{signal_channels}));
}

void configure_test_signal_channels(ebur128::state& s, unsigned int channels) {
  if (channels != 5)
    return;

  s.set_channel(ebur128::channel{0}, EBUR128_LEFT);
  s.set_channel(ebur128::channel{1}, EBUR128_RIGHT);
  s.set_channel(ebur128::channel{2}, EBUR128_CENTER);
  s.set_channel(ebur128::channel{3}, EBUR128_LEFT_SURROUND);
  s.set_channel(ebur128::channel{4}, EBUR128_RIGHT_SURROUND);
}

ebur128::mode test_signal_mode(ebur128::mode mode, bool histogram) {
  return histogram ? mode | ebur128::mode::histogram : mode;
}

const char* test_signal_mode_name(bool histogram) {
  return histogram ? "histogram" : "plain";
}

void print_test_signal_success(const char* metric,
                               int signal,
                               bool histogram,
                               double actual,
                               double expected,
                               const char* item = nullptr,
                               int item_number = 0) {
  std::printf("PASS signal %d", signal);
  if (item != nullptr)
    std::printf(" %s %d", item, item_number);
  std::printf(" (%s): %s = %.3f (expected %.3f)\n",
              test_signal_mode_name(histogram), metric, actual, expected);
}

void print_test_signal_constant_success(const char* metric,
                                        int signal,
                                        bool histogram,
                                        double expected,
                                        std::size_t readings,
                                        double minimum,
                                        double maximum) {
  std::printf("PASS signal %d (%s): %s expected %.3f, %zu readings, "
              "range [%.3f, %.3f]\n",
              signal, test_signal_mode_name(histogram), metric, expected,
              readings, minimum, maximum);
}

void print_test_signal_lra_success(int signal,
                                   bool histogram,
                                   double actual,
                                   double expected) {
  std::printf("PASS LRA signal %d (%s): LRA = %.3f (expected %.3f +/- %.1f)\n",
              signal, test_signal_mode_name(histogram), actual, expected,
              test_lra_tolerance);
}

bool check_test_signal_loudness(const char* metric,
                                int signal,
                                int variant,
                                bool histogram,
                                double actual,
                                double expected) {
  if (std::fabs(actual - expected) <= test_signal_tolerance)
    return true;

  const char* mode_name = test_signal_mode_name(histogram);
  if (variant != 0) {
    std::fprintf(stderr,
                 "signal %d variant %d (%s): %s expected %.3f, got %.3f\n",
                 signal, variant, mode_name, metric, expected, actual);
  } else {
    std::fprintf(stderr, "signal %d (%s): %s expected %.3f, got %.3f\n", signal,
                 mode_name, metric, expected, actual);
  }
  return false;
}

bool check_and_print_test_signal_loudness(const char* metric,
                                          int signal,
                                          int variant,
                                          bool histogram,
                                          double actual,
                                          double expected) {
  if (!check_test_signal_loudness(metric, signal, variant, histogram, actual,
                                  expected))
    return false;

  print_test_signal_success(metric, signal, histogram, actual, expected,
                            variant != 0 ? "variant" : nullptr, variant);
  return true;
}

bool check_test_signal_lra(int signal,
                           bool histogram,
                           double actual,
                           double expected) {
  if (std::fabs(actual - expected) <= test_lra_tolerance) {
    print_test_signal_lra_success(signal, histogram, actual, expected);
    return true;
  }

  std::fprintf(stderr, "LRA signal %d (%s): expected %.3f +/- %.1f, got %.3f\n",
               signal, test_signal_mode_name(histogram), expected,
               test_lra_tolerance, actual);
  return false;
}

double
max_test_signal_loudness(ebur128::state& s,
                         const ebur128::test_signals::test_signal& signal,
                         std::size_t threshold_frames,
                         auto measure) {
  const auto total_frames = signal.frames();
  double maximum = -std::numeric_limits<double>::infinity();

  for (std::size_t first = 0; first < total_frames;
       first += test_signal_block_frames) {
    const auto frames =
        std::min(test_signal_block_frames, total_frames - first);

    ::add_test_signal_frames(s, signal, first, frames);

    if (first + frames >= threshold_frames)
      maximum = std::max(maximum, measure());
  }

  return maximum;
}

bool check_test_signal_constant(
    ebur128::state& s,
    const ebur128::test_signals::test_signal& signal,
    std::size_t threshold_frames,
    const char* metric,
    int number,
    int variant,
    bool histogram,
    double expected,
    auto measure) {
  const auto total_frames = signal.frames();
  std::size_t readings = 0;
  double minimum = std::numeric_limits<double>::infinity();
  double maximum = -std::numeric_limits<double>::infinity();

  for (std::size_t first = 0; first < total_frames;
       first += test_signal_block_frames) {
    const auto frames =
        std::min(test_signal_block_frames, total_frames - first);

    ::add_test_signal_frames(s, signal, first, frames);

    if (first + frames < threshold_frames)
      continue;

    const double actual = measure();
    ++readings;
    minimum = std::min(minimum, actual);
    maximum = std::max(maximum, actual);

    if (!::check_test_signal_loudness(metric, number, variant, histogram,
                                      actual, expected)) {
      return false;
    }
  }

  if (readings != 0) {
    ::print_test_signal_constant_success(metric, number, histogram, expected,
                                         readings, minimum, maximum);
  }

  return true;
}

bool check_test_signal_m_s_constant(
    ebur128::state& s,
    const ebur128::test_signals::test_signal& signal,
    int number,
    bool histogram,
    double expected,
    auto momentary,
    auto shortterm) {
  const auto total_frames = signal.frames();
  const auto momentary_threshold = signal.frames_for_seconds(0.4);
  const auto shortterm_threshold = signal.frames_for_seconds(3.0);
  bool momentary_ok = true;
  bool shortterm_ok = true;
  std::size_t momentary_readings = 0;
  std::size_t shortterm_readings = 0;
  double momentary_minimum = std::numeric_limits<double>::infinity();
  double momentary_maximum = -std::numeric_limits<double>::infinity();
  double shortterm_minimum = std::numeric_limits<double>::infinity();
  double shortterm_maximum = -std::numeric_limits<double>::infinity();

  for (std::size_t first = 0; first < total_frames;
       first += test_signal_block_frames) {
    const auto frames =
        std::min(test_signal_block_frames, total_frames - first);

    ::add_test_signal_frames(s, signal, first, frames);

    const auto end = first + frames;

    if (end >= momentary_threshold && momentary_ok) {
      const double actual = momentary();
      ++momentary_readings;
      momentary_minimum = std::min(momentary_minimum, actual);
      momentary_maximum = std::max(momentary_maximum, actual);
      momentary_ok = ::check_test_signal_loudness("M", number, 0, histogram,
                                                  actual, expected);
    }
    if (end >= shortterm_threshold && shortterm_ok) {
      const double actual = shortterm();
      ++shortterm_readings;
      shortterm_minimum = std::min(shortterm_minimum, actual);
      shortterm_maximum = std::max(shortterm_maximum, actual);
      shortterm_ok = ::check_test_signal_loudness("S", number, 0, histogram,
                                                  actual, expected);
    }
  }

  if (momentary_ok && momentary_readings != 0) {
    ::print_test_signal_constant_success("M", number, histogram, expected,
                                         momentary_readings, momentary_minimum,
                                         momentary_maximum);
  }
  if (shortterm_ok && shortterm_readings != 0) {
    ::print_test_signal_constant_success("S", number, histogram, expected,
                                         shortterm_readings, shortterm_minimum,
                                         shortterm_maximum);
  }

  return momentary_ok && shortterm_ok;
}

bool check_test_signal_segment_maxima(
    ebur128::state& s,
    const ebur128::test_signals::test_signal& signal,
    double segment_seconds,
    std::size_t threshold_frames,
    const char* metric,
    int number,
    bool histogram,
    auto measure) {
  const auto total_frames = signal.frames();
  const auto segment_frames = signal.frames_for_seconds(segment_seconds);
  const auto segment_count = total_frames / segment_frames;
  std::vector<double> maxima(segment_count,
                             -std::numeric_limits<double>::infinity());

  for (std::size_t first = 0; first < total_frames;
       first += test_signal_block_frames) {
    const auto frames =
        std::min(test_signal_block_frames, total_frames - first);

    ::add_test_signal_frames(s, signal, first, frames);

    if (first + frames < threshold_frames)
      continue;

    const auto segment = first / segment_frames;
    if (segment >= segment_count)
      continue;

    maxima[segment] = std::max(maxima[segment], measure());
  }

  bool ok = true;
  for (std::size_t segment = 0; segment < segment_count; ++segment) {
    const double actual = maxima[segment];
    const double expected = -38.0 + static_cast<double>(segment);

    if (::check_test_signal_loudness(metric, number, 0, histogram, actual,
                                     expected)) {
      ::print_test_signal_success(metric, number, histogram, actual, expected,
                                  "segment", static_cast<int>(segment));
    } else {
      ok = false;
    }
  }
  return ok;
}

bool test_ebur128_signals_1_to_6() {
  bool ok = true;

  for (int number = 1; number <= 6; ++number) {
    const auto signal = test_signal{test_signal::kind::ebu3341, number};
    const double expected = number == 2 ? -33.0 : -23.0;

    for (int h = 0; h != 2; ++h) {
      const bool histogram = h != 0;
      auto mode = ebur128::mode::i;
      if (number <= 2)
        mode = mode | ebur128::mode::m | ebur128::mode::s;
      mode = ::test_signal_mode(mode, histogram);

      ebur128::state s(ebur128::channel{signal.channels()},
                       ebur128::samplerate{signal.samplerate()}, mode);
      ::configure_test_signal_channels(s, signal.channels());

      if (number <= 2) {
        ok &= ::check_test_signal_m_s_constant(
            s, signal, number, histogram, expected,
            [&] { return s.loudness_momentary(); },
            [&] { return s.loudness_shortterm(); });
      } else {
        ::add_test_signal_frames(s, signal, 0, signal.frames());
      }

      ok &= ::check_and_print_test_signal_loudness(
          "I", number, 0, histogram, s.loudness_global(), expected);
    }
  }

  return ok;
}

bool test_ebur128_signal_9() {
  const auto signal = test_signal{test_signal::kind::ebu3341, 9};
  const auto threshold_frames = signal.frames_for_seconds(3.0);
  bool ok = true;

  for (int h = 0; h != 2; ++h) {
    const bool histogram = h != 0;
    ebur128::state s(ebur128::channel{signal.channels()},
                     ebur128::samplerate{signal.samplerate()},
                     ::test_signal_mode(ebur128::mode::s, histogram));
    ::configure_test_signal_channels(s, signal.channels());
    ok &= ::check_test_signal_constant(s, signal, threshold_frames, "S", 9, 0,
                                       histogram, -23.0,
                                       [&] { return s.loudness_shortterm(); });
  }

  return ok;
}

bool test_ebur128_signal_10() {
  bool ok = true;

  for (int variant = 1; variant <= 20; ++variant) {
    const auto signal = test_signal{test_signal::kind::ebu3341, 10, variant};
    const auto threshold_frames = signal.frames_for_seconds(3.0);

    for (int h = 0; h != 2; ++h) {
      const bool histogram = h != 0;
      ebur128::state s(ebur128::channel{signal.channels()},
                       ebur128::samplerate{signal.samplerate()},
                       ::test_signal_mode(ebur128::mode::s, histogram));
      ::configure_test_signal_channels(s, signal.channels());
      const double maximum = ::max_test_signal_loudness(
          s, signal, threshold_frames, [&] { return s.loudness_shortterm(); });
      ok &= ::check_and_print_test_signal_loudness("max S", 10, variant,
                                                   histogram, maximum, -23.0);
    }
  }

  return ok;
}

bool test_ebur128_signal_11() {
  const auto signal = test_signal{test_signal::kind::ebu3341, 11};
  const auto threshold_frames = signal.frames_for_seconds(3.0);
  bool ok = true;

  for (int h = 0; h != 2; ++h) {
    const bool histogram = h != 0;
    ebur128::state s(ebur128::channel{signal.channels()},
                     ebur128::samplerate{signal.samplerate()},
                     ::test_signal_mode(ebur128::mode::s, histogram));
    ::configure_test_signal_channels(s, signal.channels());
    ok &= ::check_test_signal_segment_maxima(
        s, signal, 6.0, threshold_frames, "max S", 11, histogram,
        [&] { return s.loudness_shortterm(); });
  }

  return ok;
}

bool test_ebur128_signal_12() {
  const auto signal = test_signal{test_signal::kind::ebu3341, 12};
  const auto threshold_frames = signal.frames_for_seconds(1.0);
  bool ok = true;

  for (int h = 0; h != 2; ++h) {
    const bool histogram = h != 0;
    ebur128::state s(ebur128::channel{signal.channels()},
                     ebur128::samplerate{signal.samplerate()},
                     ::test_signal_mode(ebur128::mode::m, histogram));
    ::configure_test_signal_channels(s, signal.channels());
    ok &= ::check_test_signal_constant(s, signal, threshold_frames, "M", 12, 0,
                                       histogram, -23.0,
                                       [&] { return s.loudness_momentary(); });
  }

  return ok;
}

bool test_ebur128_signal_13() {
  bool ok = true;

  for (int variant = 1; variant <= 20; ++variant) {
    const auto signal = test_signal{test_signal::kind::ebu3341, 13, variant};
    const auto threshold_frames = signal.frames_for_seconds(0.4);

    for (int h = 0; h != 2; ++h) {
      const bool histogram = h != 0;
      ebur128::state s(ebur128::channel{signal.channels()},
                       ebur128::samplerate{signal.samplerate()},
                       ::test_signal_mode(ebur128::mode::m, histogram));
      ::configure_test_signal_channels(s, signal.channels());
      const double maximum = ::max_test_signal_loudness(
          s, signal, threshold_frames, [&] { return s.loudness_momentary(); });
      ok &= ::check_and_print_test_signal_loudness("max M", 13, variant,
                                                   histogram, maximum, -23.0);
    }
  }

  return ok;
}

bool test_ebur128_signal_14() {
  const auto signal = test_signal{test_signal::kind::ebu3341, 14};
  const auto threshold_frames = signal.frames_for_seconds(0.4);
  bool ok = true;

  for (int h = 0; h != 2; ++h) {
    const bool histogram = h != 0;
    ebur128::state s(ebur128::channel{signal.channels()},
                     ebur128::samplerate{signal.samplerate()},
                     ::test_signal_mode(ebur128::mode::m, histogram));
    ::configure_test_signal_channels(s, signal.channels());
    ok &= ::check_test_signal_segment_maxima(
        s, signal, 0.8, threshold_frames, "max M", 14, histogram,
        [&] { return s.loudness_momentary(); });
  }

  return ok;
}

bool test_ebur128_lra() {
  const struct {
    int number;
    double expected;
  } cases[] = {
      {1, 10.0},
      {2, 5.0},
      {3, 20.0},
      {4, 15.0},
  };
  bool ok = true;

  for (const auto& test_case : cases) {
    const auto signal =
        test_signal{test_signal::kind::ebu3342, test_case.number};

    for (int h = 0; h != 2; ++h) {
      const bool histogram = h != 0;
      ebur128::state s(ebur128::channel{signal.channels()},
                       ebur128::samplerate{signal.samplerate()},
                       ::test_signal_mode(ebur128::mode::lra, histogram));
      ::configure_test_signal_channels(s, signal.channels());
      ::add_test_signal_frames(s, signal, 0, signal.frames());
      ok &= ::check_test_signal_lra(test_case.number, histogram,
                                    s.loudness_range(), test_case.expected);
    }
  }

  return ok;
}

bool test_ebur128_signals() {
  bool ok = true;
  ok &= ::test_ebur128_signals_1_to_6();
  ok &= ::test_ebur128_signal_9();
  ok &= ::test_ebur128_signal_10();
  ok &= ::test_ebur128_signal_11();
  ok &= ::test_ebur128_signal_12();
  ok &= ::test_ebur128_signal_13();
  ok &= ::test_ebur128_signal_14();
  ok &= ::test_ebur128_lra();
  return ok;
}

} // namespace

int main() {
  assert(::test_ebur128_simple());
  assert(::test_ebur128_threaded_init());
#ifdef EBUR128_TEST_PERFORMANCE
  assert(::test_ebur128_ftz_performance());
#endif
  assert(::test_ebur128_init());
  assert(::test_ebur128_set_channel());
  assert(::test_ebur128_loudness_window_overflow());
  assert(::test_ebur128_set_max_window_large_allocation());
  assert(::test_ebur128_signals());
}

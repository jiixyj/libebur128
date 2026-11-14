#undef NDEBUG
#include <cassert>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <ebur128.h>
#include <ebur128.test.h>
#include <ebur128.test_signals.h>

namespace ebur128 {
using namespace ebur128::test;
using ebur128::test_signals::sample_type;
} // namespace ebur128

static bool test_simple() {
  ebur128::state s(ebur128::channel{2}, ebur128::samplerate{44100},
                   ebur128::mode::i);
  (void) s;
  return true;
}

static bool test_threaded_init() {
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

bool test_ftz_performance() {
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

static bool test_init() {
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

static bool test_set_channel() {
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

static bool test_dual_mono() {
  constexpr std::size_t samplerate = 48000;
  constexpr std::size_t frames = samplerate;
  constexpr double frequency = 1000.0;
  constexpr double amplitude = 0.25;

  std::vector<double> input(frames);
  for (std::size_t i = 0; i < frames; ++i) {
    input[i] = amplitude * std::sin(2.0 * std::numbers::pi * frequency *
                                    static_cast<double>(i) /
                                    static_cast<double>(samplerate));
  }

  ebur128::state single(ebur128::channel{1}, ebur128::samplerate{samplerate},
                        ebur128::mode::i);
  single.add_frames(ebur128::audio_data_view(input, ebur128::channel{1}));

  ebur128::state dual(ebur128::channel{1}, ebur128::samplerate{samplerate},
                      ebur128::mode::i);
  dual.set_channel(ebur128::channel{0}, EBUR128_DUAL_MONO);
  dual.add_frames(ebur128::audio_data_view(input, ebur128::channel{1}));

  assert(std::fabs((dual.loudness_global() - single.loudness_global()) -
                   10.0 * std::log10(2.0)) <= 1e-6);
  return true;
}

static bool test_surround_channels() {
  constexpr std::size_t samplerate = 48000;
  constexpr std::size_t frames = samplerate;
  constexpr double frequency = 1000.0;
  constexpr double amplitude = 0.25;

  std::vector<double> input(frames);
  for (std::size_t i = 0; i < frames; ++i) {
    input[i] = amplitude * std::sin(2.0 * std::numbers::pi * frequency *
                                    static_cast<double>(i) /
                                    static_cast<double>(samplerate));
  }

  ebur128::state left(ebur128::channel{1}, ebur128::samplerate{samplerate},
                      ebur128::mode::i);
  left.add_frames(ebur128::audio_data_view(input, ebur128::channel{1}));

  for (auto channel : {EBUR128_Mp110, EBUR128_Mm110, EBUR128_Mp060,
                       EBUR128_Mm060, EBUR128_Mp090, EBUR128_Mm090}) {
    ebur128::state surround(ebur128::channel{1},
                            ebur128::samplerate{samplerate}, ebur128::mode::i);
    surround.set_channel(ebur128::channel{0}, channel);
    surround.add_frames(ebur128::audio_data_view(input, ebur128::channel{1}));

    assert(std::fabs((surround.loudness_global() - left.loudness_global()) -
                     10.0 * std::log10(1.41)) <= 1e-6);
  }

  return true;
}

static bool test_loudness_window() {
  {
    ebur128::state s(ebur128::channel{1}, ebur128::samplerate{48000},
                     ebur128::mode::m);

    s.set_max_window(std::chrono::milliseconds{6000});

    assert(s.loudness_window(std::chrono::milliseconds{6000}) == -HUGE_VAL);

    try {
      s.loudness_window(std::chrono::milliseconds{6001});
      assert(false);
    } catch (const ebur128::invalid_mode_error&) {
    }
  }

  return true;
}

// This tests for a (fixed) unsigned integer overflow in
// `ebur128_loudness_window`.
static bool test_loudness_window_overflow() {
  ebur128::state s(ebur128::channel{1}, ebur128::samplerate{48000},
                   ebur128::mode::m);

  s.set_max_window(std::chrono::milliseconds{89479});

  std::vector<short> input(19200);
  for (std::size_t i = 0; i < input.size(); ++i) {
    input[i] = static_cast<short>(32767.0 *
                                  std::sin(2.0 * std::numbers::pi * 1000.0 *
                                           static_cast<double>(i) / 48000.0));
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

// Window cannot get smaller than the default 400ms/3000ms.
static bool test_set_max_window() {
  {
    ebur128::state s(ebur128::channel{2}, ebur128::samplerate{48000},
                     ebur128::mode::m);
    try {
      s.set_max_window(std::chrono::milliseconds{100});
      assert(false);
    } catch (const ebur128::no_change_error&) {
    }
  }
  {
    ebur128::state s(ebur128::channel{2}, ebur128::samplerate{48000},
                     ebur128::mode::s);
    try {
      s.set_max_window(std::chrono::milliseconds{100});
      assert(false);
    } catch (const ebur128::no_change_error&) {
    }
  }
  {
    ebur128::state s(ebur128::channel{2}, ebur128::samplerate{48000},
                     ebur128::mode::i);
    try {
      s.set_max_window(std::chrono::milliseconds{100});
      assert(false);
    } catch (const ebur128::no_change_error&) {
    }
  }
  return true;
}

// 48 kHz mono, 2,796,200 ms allocates just under 1 GiB.
static bool test_set_max_window_large_allocation() {
  ebur128::state s(ebur128::channel{1}, ebur128::samplerate{48000},
                   ebur128::mode::m);
  s.set_max_window(std::chrono::milliseconds{2'796'200});
  return true;
}

static bool test_integer_overflow() {
  bool ok = true;
  const auto expect_nomem = [](const char* name, auto&& f) {
    try {
      f();
    } catch (const ebur128::nomem_error&) {
      return true;
    } catch (const ebur128::error& e) {
      std::fprintf(stderr, "%s: unexpected error: %s\n", name, e.what());
      return false;
    }
    std::fprintf(stderr, "%s: expected nomem_error\n", name);
    return false;
  };

  {
    ebur128::state s(ebur128::channel{1}, ebur128::samplerate{2822400},
                     ebur128::mode::m);

    constexpr auto a_hi = 2822400UL / 1000UL;
    constexpr auto q = std::numeric_limits<unsigned long>::max() / 1000UL;
    constexpr auto b_hi = q / a_hi;

    const auto term1_window = (b_hi + 1UL) * 1000UL;
    ok &= expect_nomem("mul_div_1000 term1", [&] {
      s.set_max_window(std::chrono::milliseconds{
          static_cast<std::chrono::milliseconds::rep>(term1_window)});
    });

    const auto add_term_window = b_hi * 1000UL + 999UL;
    ok &= expect_nomem("mul_div_1000 add term", [&] {
      s.set_max_window(std::chrono::milliseconds{
          static_cast<std::chrono::milliseconds::rep>(add_term_window)});
    });
  }

  if constexpr (sizeof(std::size_t) <= sizeof(unsigned long)) {
    constexpr auto samplerate = 2822000UL;
    constexpr auto a_hi = samplerate / 1000UL;
    constexpr auto q = std::numeric_limits<unsigned long>::max() / 1000UL;
    constexpr auto b_hi = q / a_hi;
    const auto term1 = a_hi * b_hi * 1000UL;
    const auto leftover = std::numeric_limits<unsigned long>::max() - term1;
    const auto b_lo = std::min(999UL, leftover / a_hi);
    const auto window = b_hi * 1000UL + b_lo;

    ebur128::state s(ebur128::channel{1}, ebur128::samplerate{samplerate},
                     ebur128::mode::m);
    ok &= expect_nomem("calculate_interval_frames", [&] {
      s.set_max_window(std::chrono::milliseconds{
          static_cast<std::chrono::milliseconds::rep>(window)});
    });
  }

  if constexpr (sizeof(std::size_t) <= sizeof(unsigned long)) {
    constexpr std::size_t size = std::size_t{64} * sizeof(double);
    constexpr auto max = std::numeric_limits<std::size_t>::max();
    const auto min_overflow_frames = max / size + 1;
    const auto frames = (min_overflow_frames + 1) / 2 * 2;
    const auto window = frames / 2 * 125;

    ebur128::state s(ebur128::channel{64}, ebur128::samplerate{16},
                     ebur128::mode::m);
    ok &= expect_nomem("safe_size_mul", [&] {
      s.set_max_window(std::chrono::milliseconds{
          static_cast<std::chrono::milliseconds::rep>(window)});
    });
  }

  return ok;
}

namespace {

template <ebur128::sample_type T>
using test_signal = ebur128::test_signals::test_signal<T>;

constexpr double test_signal_tolerance = 0.1;
constexpr double test_lra_tolerance = 1.0;
constexpr std::size_t test_signal_block_frames =
    static_cast<std::size_t>(test_signal<double>::default_samplerate) / 100;

template <ebur128::sample_type T>
void add_test_signal_frames(ebur128::state& s,
                            const test_signal<T>& signal,
                            std::size_t first_frame,
                            std::size_t frames) {
  const auto signal_channels = signal.channels();
  const auto channels = static_cast<std::size_t>(signal_channels);
  const auto samples = signal.samples();
  s.add_frames(ebur128::audio_data_view<T>(
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

double max_test_signal_loudness(ebur128::state& s,
                                const auto& signal,
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

bool check_test_signal_constant(ebur128::state& s,
                                const auto& signal,
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

bool check_test_signal_m_s_constant(ebur128::state& s,
                                    const auto& signal,
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

bool check_test_signal_segment_maxima(ebur128::state& s,
                                      const auto& signal,
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

template <ebur128::sample_type T, bool histogram, bool m_s_only = false>
bool test_signals_1_to_6(int number, int variant = 0) {
  bool ok = true;

  const test_signal<T> signal(ebur128::test_signals::test_signal_kind::ebu3341,
                              number, variant);
  const double expected = number == 2 ? -33.0 : -23.0;

  auto mode = ebur128::mode::i;
  if (number <= 2)
    mode = mode | ebur128::mode::m | ebur128::mode::s;
  mode = ::test_signal_mode(mode, histogram);

  ebur128::state s(ebur128::channel{signal.channels()},
                   ebur128::samplerate{signal.samplerate()}, mode);
  ::configure_test_signal_channels(s, signal.channels());
  if (m_s_only) {
    // If we only check m and s, we can limit the history.
    s.set_max_history(std::chrono::milliseconds{400});
  }

  if (number <= 2) {
    ok &= ::check_test_signal_m_s_constant(
        s, signal, number, histogram, expected,
        [&] { return s.loudness_momentary(); },
        [&] { return s.loudness_shortterm(); });
  } else {
    ::add_test_signal_frames(s, signal, 0, signal.frames());
  }

  if (!m_s_only) {
    ok &= ::check_and_print_test_signal_loudness("I", number, 0, histogram,
                                                 s.loudness_global(), expected);
  }

  return ok;
}

template <ebur128::sample_type T, bool histogram> bool test_signal_1() {
  return ::test_signals_1_to_6<T, histogram, false>(1) &&
         ::test_signals_1_to_6<T, histogram, true>(1);
}
template <ebur128::sample_type T, bool histogram> bool test_signal_2() {
  return ::test_signals_1_to_6<T, histogram, false>(2) &&
         ::test_signals_1_to_6<T, histogram, true>(2);
}
template <ebur128::sample_type T, bool histogram> bool test_signal_3() {
  return ::test_signals_1_to_6<T, histogram>(3);
}
template <ebur128::sample_type T, bool histogram> bool test_signal_4() {
  return ::test_signals_1_to_6<T, histogram>(4);
}
template <ebur128::sample_type T, bool histogram> bool test_signal_5() {
  return ::test_signals_1_to_6<T, histogram>(5);
}
template <ebur128::sample_type T, bool histogram> bool test_signal_6() {
  return ::test_signals_1_to_6<T, histogram>(6, 1);
}
// Relies on the default channel map (see `ebur128_set_channel`).
template <ebur128::sample_type T, bool histogram> bool test_signal_6_51() {
  return ::test_signals_1_to_6<T, histogram>(6, 2);
}

template <ebur128::sample_type T, bool histogram> bool test_signal_9() {
  const test_signal<T> signal(ebur128::test_signals::test_signal_kind::ebu3341,
                              9);
  const auto threshold_frames = signal.frames_for_seconds(3.0);
  bool ok = true;

  ebur128::state s(ebur128::channel{signal.channels()},
                   ebur128::samplerate{signal.samplerate()},
                   ::test_signal_mode(ebur128::mode::s, histogram));
  ::configure_test_signal_channels(s, signal.channels());
  ok &= ::check_test_signal_constant(s, signal, threshold_frames, "S", 9, 0,
                                     histogram, -23.0,
                                     [&] { return s.loudness_shortterm(); });

  return ok;
}

template <ebur128::sample_type T, bool histogram> bool test_signal_10() {
  bool ok = true;

  for (int variant = 1; variant <= 20; ++variant) {
    const test_signal<T> signal(
        ebur128::test_signals::test_signal_kind::ebu3341, 10, variant);
    const auto threshold_frames = signal.frames_for_seconds(3.0);

    ebur128::state s(ebur128::channel{signal.channels()},
                     ebur128::samplerate{signal.samplerate()},
                     ::test_signal_mode(ebur128::mode::s, histogram));
    ::configure_test_signal_channels(s, signal.channels());
    const double maximum = ::max_test_signal_loudness(
        s, signal, threshold_frames, [&] { return s.loudness_shortterm(); });
    ok &= ::check_and_print_test_signal_loudness("max S", 10, variant,
                                                 histogram, maximum, -23.0);
  }

  return ok;
}

template <ebur128::sample_type T, bool histogram> bool test_signal_11() {
  const test_signal<T> signal(ebur128::test_signals::test_signal_kind::ebu3341,
                              11);
  const auto threshold_frames = signal.frames_for_seconds(3.0);
  bool ok = true;

  ebur128::state s(ebur128::channel{signal.channels()},
                   ebur128::samplerate{signal.samplerate()},
                   ::test_signal_mode(ebur128::mode::s, histogram));
  ::configure_test_signal_channels(s, signal.channels());
  ok &= ::check_test_signal_segment_maxima(
      s, signal, 6.0, threshold_frames, "max S", 11, histogram,
      [&] { return s.loudness_shortterm(); });

  return ok;
}

template <ebur128::sample_type T, bool histogram> bool test_signal_12() {
  const test_signal<T> signal(ebur128::test_signals::test_signal_kind::ebu3341,
                              12);
  const auto threshold_frames = signal.frames_for_seconds(1.0);
  bool ok = true;

  ebur128::state s(ebur128::channel{signal.channels()},
                   ebur128::samplerate{signal.samplerate()},
                   ::test_signal_mode(ebur128::mode::m, histogram));
  ::configure_test_signal_channels(s, signal.channels());
  ok &= ::check_test_signal_constant(s, signal, threshold_frames, "M", 12, 0,
                                     histogram, -23.0,
                                     [&] { return s.loudness_momentary(); });

  return ok;
}

template <ebur128::sample_type T, bool histogram> bool test_signal_13() {
  bool ok = true;

  for (int variant = 1; variant <= 20; ++variant) {
    const test_signal<T> signal(
        ebur128::test_signals::test_signal_kind::ebu3341, 13, variant);
    const auto threshold_frames = signal.frames_for_seconds(0.4);

    ebur128::state s(ebur128::channel{signal.channels()},
                     ebur128::samplerate{signal.samplerate()},
                     ::test_signal_mode(ebur128::mode::m, histogram));
    ::configure_test_signal_channels(s, signal.channels());
    const double maximum = ::max_test_signal_loudness(
        s, signal, threshold_frames, [&] { return s.loudness_momentary(); });
    ok &= ::check_and_print_test_signal_loudness("max M", 13, variant,
                                                 histogram, maximum, -23.0);
  }

  return ok;
}

template <ebur128::sample_type T, bool histogram> bool test_signal_14() {
  const test_signal<T> signal(ebur128::test_signals::test_signal_kind::ebu3341,
                              14);
  const auto threshold_frames = signal.frames_for_seconds(0.4);
  bool ok = true;

  ebur128::state s(ebur128::channel{signal.channels()},
                   ebur128::samplerate{signal.samplerate()},
                   ::test_signal_mode(ebur128::mode::m, histogram));
  ::configure_test_signal_channels(s, signal.channels());
  ok &= ::check_test_signal_segment_maxima(
      s, signal, 0.8, threshold_frames, "max M", 14, histogram,
      [&] { return s.loudness_momentary(); });

  return ok;
}

template <ebur128::sample_type T, bool histogram>
bool run_lra_case(int number, double expected) {
  const test_signal<T> signal(ebur128::test_signals::test_signal_kind::ebu3342,
                              number);

  ebur128::state s(ebur128::channel{signal.channels()},
                   ebur128::samplerate{signal.samplerate()},
                   ::test_signal_mode(ebur128::mode::lra, histogram));
  ::configure_test_signal_channels(s, signal.channels());
  ::add_test_signal_frames(s, signal, 0, signal.frames());

  return ::check_test_signal_lra(number, histogram, s.loudness_range(),
                                 expected);
}

template <ebur128::sample_type T, bool histogram> bool test_lra_1() {
  return ::run_lra_case<T, histogram>(1, 10.0);
}

template <ebur128::sample_type T, bool histogram> bool test_lra_2() {
  return ::run_lra_case<T, histogram>(2, 5.0);
}

template <ebur128::sample_type T, bool histogram> bool test_lra_3() {
  return ::run_lra_case<T, histogram>(3, 20.0);
}

template <ebur128::sample_type T, bool histogram> bool test_lra_4() {
  return ::run_lra_case<T, histogram>(4, 15.0);
}

} // namespace

static bool test_version() {
  return ebur128::state::version() == std::tuple{1, 2, 6};
}

static bool test_change_parameters() {
  {
    ebur128::state s(ebur128::channel{2}, ebur128::samplerate{48000},
                     ebur128::mode::i);

    try {
      s.change_parameters(ebur128::channel{2}, ebur128::samplerate{48000});
      assert(false);
    } catch (const ebur128::no_change_error&) {
    }

    try {
      s.change_parameters(ebur128::channel{0}, ebur128::samplerate{48000});
      assert(false);
    } catch (const ebur128::nomem_error&) {
    }

    try {
      s.change_parameters(ebur128::channel{2}, ebur128::samplerate{2822401});
      assert(false);
    } catch (const ebur128::nomem_error&) {
    }

    s.change_parameters(ebur128::channel{2}, ebur128::samplerate{44100});
    s.change_parameters(ebur128::channel{4}, ebur128::samplerate{44100});
    s.change_parameters(ebur128::channel{2}, ebur128::samplerate{48000});

    try {
      s.change_parameters(ebur128::channel{2}, ebur128::samplerate{48000});
      assert(false);
    } catch (const ebur128::no_change_error&) {
    }
  }
  return true;
}

static bool test_global_loudness() {
  {
    ebur128::state s(ebur128::channel{2}, ebur128::samplerate{48000},
                     ebur128::mode::m);
    try {
      (void) s.loudness_global();
      assert(false);
    } catch (const ebur128::invalid_mode_error&) {
    }
  }
  {
    ebur128::state s(ebur128::channel{2}, ebur128::samplerate{48000},
                     ebur128::mode::s);
    try {
      (void) s.loudness_global();
      assert(false);
    } catch (const ebur128::invalid_mode_error&) {
    }
  }
  {
    ebur128::state s(ebur128::channel{2}, ebur128::samplerate{48000},
                     ebur128::mode::i);
    assert(s.loudness_global() == -HUGE_VAL);
  }
  return true;
}

static bool test_global_loudness_quiet() {
  constexpr std::size_t samplerate = 48000;
  constexpr std::size_t frames = samplerate;
  constexpr double frequency = 1000.0;
  const double amplitude = std::pow(10.0, -64.0 / 20.0);

  std::vector<double> input(frames * 2);
  for (std::size_t i = 0; i < frames; ++i) {
    const double sample =
        amplitude *
        std::sin(2.0 * std::numbers::pi * frequency * static_cast<double>(i) /
                 static_cast<double>(samplerate));
    input[i * 2] = sample;
    input[i * 2 + 1] = sample;
  }

  ebur128::state s(ebur128::channel{2}, ebur128::samplerate{samplerate},
                   ebur128::mode::i);
  s.add_frames(ebur128::audio_data_view(input, ebur128::channel{2}));

  assert(std::fabs(s.loudness_global() - -64.0) <= 0.1);
  return true;
}

static bool test_global_loudness_multiple() {
  {
    std::vector<ebur128::state> states = {};
    states.emplace_back(ebur128::channel{2}, ebur128::samplerate{48000},
                        ebur128::mode::i);
    states.emplace_back();

    const test_signal<double> signal(
        ebur128::test_signals::test_signal_kind::ebu3341, 3);
    ::add_test_signal_frames(states[0], signal, 0, signal.frames());

    // `ebur128_loudness_global_multiple` skips over NULL pointers
    // in the given array
    {
      double result = ebur128::state::loudness_global_multiple(states);
      assert(std::fabs(result - -23.0) <= test_signal_tolerance);
    }
  }

  return true;
}

static bool test_loudness_range_quiet() {
  constexpr std::size_t samplerate = 48000;
  constexpr std::size_t frames = 3 * samplerate;
  constexpr double frequency = 1000.0;
  const double amplitude = std::pow(10.0, -71.0 / 20.0);

  std::vector<double> input(frames * 2);
  for (std::size_t i = 0; i < frames; ++i) {
    const double sample =
        amplitude *
        std::sin(2.0 * std::numbers::pi * frequency * static_cast<double>(i) /
                 static_cast<double>(samplerate));
    input[i * 2] = sample;
    input[i * 2 + 1] = sample;
  }

  ebur128::state s(ebur128::channel{2}, ebur128::samplerate{samplerate},
                   ebur128::mode::lra);
  s.add_frames(ebur128::audio_data_view(input, ebur128::channel{2}));

  assert(s.loudness_range() == 0.0);

  return true;
}

static bool test_loudness_range_multiple() {
  {
    std::vector<ebur128::state> states;
    states.emplace_back(ebur128::channel{2}, ebur128::samplerate{48000},
                        ebur128::mode::i);
    try {
      (void) ebur128::state::loudness_range_multiple(states);
      assert(false);
    } catch (const ebur128::invalid_mode_error&) {
    }
  }
  {
    std::vector<ebur128::state> states;
    states.emplace_back(ebur128::channel{2}, ebur128::samplerate{48000},
                        ebur128::mode::lra);
    states.emplace_back(ebur128::channel{2}, ebur128::samplerate{48000},
                        ebur128::mode::lra | ebur128::mode::histogram);
    try {
      (void) ebur128::state::loudness_range_multiple(states);
      assert(false);
    } catch (const ebur128::invalid_mode_error&) {
    }
  }
  {
    const test_signal<double> signal(
        ebur128::test_signals::test_signal_kind::ebu3341, 3);
    for (const bool histogram : {false, true}) {
      std::vector<ebur128::state> states;
      states.emplace_back(ebur128::channel{signal.channels()},
                          ebur128::samplerate{signal.samplerate()},
                          ::test_signal_mode(ebur128::mode::lra, histogram));
      states.emplace_back();
      ::add_test_signal_frames(states[0], signal, 0, signal.frames());

      assert(std::fabs(ebur128::state::loudness_range_multiple(states) -
                       13.0) <= test_lra_tolerance);
    }
  }
  {
    std::vector<ebur128::state> states;
    states.emplace_back();
    states.emplace_back();

    assert(std::fabs(ebur128::state::loudness_range_multiple(states) - 0.0) <=
           test_lra_tolerance);
  }
  {
    std::vector<ebur128::state> states;
    states.emplace_back(ebur128::channel{2}, ebur128::samplerate{48000},
                        ebur128::mode::lra | ebur128::mode::histogram);

    assert(std::fabs(ebur128::state::loudness_range_multiple(states) - 0.0) <=
           test_lra_tolerance);
  }
  {
    constexpr std::size_t samplerate = 48000;
    constexpr std::size_t frames = 4 * samplerate;
    constexpr double frequency = 1000.0;
    const double amplitude = std::pow(10.0, -64.0 / 20.0);

    std::vector<double> input(frames * 2);
    for (std::size_t i = 0; i < frames; ++i) {
      const double sample =
          amplitude *
          std::sin(2.0 * std::numbers::pi * frequency * static_cast<double>(i) /
                   static_cast<double>(samplerate));
      input[i * 2] = sample;
      input[i * 2 + 1] = sample;
    }

    std::vector<ebur128::state> states;
    states.emplace_back(ebur128::channel{2}, ebur128::samplerate{samplerate},
                        ebur128::mode::lra | ebur128::mode::histogram);
    states[0].add_frames(ebur128::audio_data_view(input, ebur128::channel{2}));

    assert(std::fabs(ebur128::state::loudness_range_multiple(states) - 0.0) <=
           test_lra_tolerance);
  }
  return true;
}

static bool test_loudness_range_partial_frames() {
  const test_signal<double> signal(
      ebur128::test_signals::test_signal_kind::ebu3342, 1);
  const auto initial_frames = signal.frames_for_seconds(1.0);
  const auto partial_frames = signal.frames_for_seconds(0.01);
  const auto remaining_frames =
      signal.frames() - initial_frames - partial_frames;

  ebur128::state s(ebur128::channel{signal.channels()},
                   ebur128::samplerate{signal.samplerate()},
                   ebur128::mode::lra);
  ::add_test_signal_frames(s, signal, 0, initial_frames);
  ::add_test_signal_frames(s, signal, initial_frames, partial_frames);
  ::add_test_signal_frames(s, signal, initial_frames + partial_frames,
                           remaining_frames);

  assert(std::fabs(s.loudness_range() - 10.0) <= test_lra_tolerance);
  return true;
}

static bool test_relative_threshold() {
  {
    ebur128::state s(ebur128::channel{2}, ebur128::samplerate{48000},
                     ebur128::mode::m);
    try {
      (void) s.relative_threshold();
      assert(false);
    } catch (const ebur128::invalid_mode_error&) {
    }
  }
  {
    ebur128::state s(ebur128::channel{2}, ebur128::samplerate{48000},
                     ebur128::mode::i);
    assert(s.relative_threshold() == -70.0);
  }
  {
    const test_signal<double> signal(
        ebur128::test_signals::test_signal_kind::ebu3341, 1);
    for (const bool histogram : {false, true}) {
      ebur128::state s(ebur128::channel{signal.channels()},
                       ebur128::samplerate{signal.samplerate()},
                       ::test_signal_mode(ebur128::mode::i, histogram));
      ::add_test_signal_frames(s, signal, 0, signal.frames());
      assert(std::fabs(s.relative_threshold() - -33.0) <=
             test_signal_tolerance);
    }
  }
  {
    const test_signal<double> signal(
        ebur128::test_signals::test_signal_kind::ebu3341, 4);
    for (const bool histogram : {false, true}) {
      ebur128::state s(ebur128::channel{signal.channels()},
                       ebur128::samplerate{signal.samplerate()},
                       ::test_signal_mode(ebur128::mode::i, histogram));
      ::configure_test_signal_channels(s, signal.channels());
      ::add_test_signal_frames(s, signal, 0, signal.frames());
      assert(std::fabs(s.relative_threshold() - -34.2) <=
             test_signal_tolerance);
    }
  }
  return true;
}

static bool test_momentary_loudness() {
  {
    ebur128::state s(ebur128::channel{2}, ebur128::samplerate{48000},
                     ebur128::mode::m);

    assert(s.loudness_momentary() == -HUGE_VAL);
  }

  {
    ebur128::state s(ebur128::channel{2}, ebur128::samplerate{48000},
                     ebur128::mode::sample_peak);

    assert(s.loudness_momentary() == -HUGE_VAL);
  }

  return true;
}

static bool test_shortterm_loudness() {
  {
    ebur128::state s(ebur128::channel{2}, ebur128::samplerate{48000},
                     ebur128::mode::s);

    assert(s.loudness_shortterm() == -HUGE_VAL);
  }

  {
    ebur128::state s(ebur128::channel{2}, ebur128::samplerate{48000},
                     ebur128::mode::sample_peak);

    try {
      (void) s.loudness_shortterm();
      assert(false);
    } catch (const ebur128::invalid_mode_error&) {
    }
  }

  return true;
}

static bool test_set_max_history() {
  // History cannot get smaller than the default 400ms (m) / 3000ms (lra).
  {
    ebur128::state s(ebur128::channel{2}, ebur128::samplerate{48000},
                     ebur128::mode::m);
    s.set_max_history(std::chrono::milliseconds{400});
    try {
      s.set_max_history(std::chrono::milliseconds{100});
      assert(false);
    } catch (const ebur128::no_change_error&) {
    }
  }
  {
    ebur128::state s(ebur128::channel{2}, ebur128::samplerate{48000},
                     ebur128::mode::lra);
    s.set_max_history(std::chrono::milliseconds{3000});
    try {
      s.set_max_history(std::chrono::milliseconds{100});
      assert(false);
    } catch (const ebur128::no_change_error&) {
    }
  }

  {
    ebur128::state s(ebur128::channel{2}, ebur128::samplerate{48000},
                     ebur128::mode::i);

    const test_signal<double> signal(
        ebur128::test_signals::test_signal_kind::ebu3341, 3);
    ::add_test_signal_frames(s, signal, 0, signal.frames());

    assert(std::fabs(s.loudness_global() - -23.0) <= test_signal_tolerance);

    // Will set history to 400ms and cut off all blocks except for a few at the
    // end.
    s.set_max_history(std::chrono::milliseconds{0});

    assert(std::fabs(s.loudness_global() - -36.0) <= test_signal_tolerance);
  }

  {
    ebur128::state s(ebur128::channel{2}, ebur128::samplerate{48000},
                     ebur128::mode::lra);

    const test_signal<double> signal(
        ebur128::test_signals::test_signal_kind::ebu3341, 3);
    ::add_test_signal_frames(s, signal, 0, signal.frames());

    assert(std::fabs(s.loudness_range() - 13.0) <= test_lra_tolerance);

    s.set_max_history(std::chrono::seconds{30});
    assert(std::fabs(s.loudness_range() - 11.3466) <= test_lra_tolerance);

    s.set_max_history(std::chrono::milliseconds{0});
    assert(std::fabs(s.loudness_range() - 0.0) <= test_lra_tolerance);
  }

  {
    ebur128::state s(ebur128::channel{2}, ebur128::samplerate{48000},
                     ebur128::mode::lra);

    s.set_max_history(std::chrono::milliseconds{0});

    const test_signal<double> signal(
        ebur128::test_signals::test_signal_kind::ebu3341, 3);
    ::add_test_signal_frames(s, signal, 0, signal.frames());

    assert(std::fabs(s.loudness_range() - 0.0) <= test_lra_tolerance);
  }

  return true;
}

static bool test_true_peak() {
  {
    ebur128::state s(ebur128::channel{2}, ebur128::samplerate{48000},
                     ebur128::mode::m);
    try {
      s.true_peak(ebur128::channel{0});
      assert(false);
    } catch (const ebur128::invalid_mode_error&) {
    }
  }

  {
    ebur128::state s(ebur128::channel{2}, ebur128::samplerate{48000},
                     ebur128::mode::true_peak);
    assert(s.true_peak(ebur128::channel{0}) == 0.0);
    assert(s.true_peak(ebur128::channel{1}) == 0.0);
    try {
      s.true_peak(ebur128::channel{2});
      assert(false);
    } catch (const ebur128::invalid_channel_index_error&) {
    }
  }

  // At 192kHz true peak is the same as sample peak.
  {
    ebur128::state s(ebur128::channel{2}, ebur128::samplerate{192000},
                     ebur128::mode::true_peak);

    const std::vector<double> first{0.25, 0.5, -0.75, -0.125};
    s.add_frames(ebur128::audio_data_view(first, ebur128::channel{2}));
    assert(s.true_peak(ebur128::channel{0}) == 0.75);
    assert(s.true_peak(ebur128::channel{1}) == 0.5);
    assert(s.sample_peak(ebur128::channel{0}) == 0.75);
    assert(s.sample_peak(ebur128::channel{1}) == 0.5);
  }

  return true;
}

static bool test_sample_peak() {
  {
    ebur128::state s(ebur128::channel{2}, ebur128::samplerate{48000},
                     ebur128::mode::m);
    try {
      s.sample_peak(ebur128::channel{0});
      assert(false);
    } catch (const ebur128::invalid_mode_error&) {
    }
  }

  {
    ebur128::state s(ebur128::channel{2}, ebur128::samplerate{48000},
                     ebur128::mode::sample_peak);
    assert(s.sample_peak(ebur128::channel{0}) == 0.0);
    assert(s.sample_peak(ebur128::channel{1}) == 0.0);
    try {
      s.sample_peak(ebur128::channel{2});
      assert(false);
    } catch (const ebur128::invalid_channel_index_error&) {
    }
  }

  return true;
}

static bool test_prev_sample_peak() {
  {
    ebur128::state s(ebur128::channel{2}, ebur128::samplerate{48000},
                     ebur128::mode::m);
    try {
      s.prev_sample_peak(ebur128::channel{0});
      assert(false);
    } catch (const ebur128::invalid_mode_error&) {
    }
  }

  {
    ebur128::state s(ebur128::channel{2}, ebur128::samplerate{48000},
                     ebur128::mode::sample_peak);
    assert(s.prev_sample_peak(ebur128::channel{0}) == 0.0);
    assert(s.prev_sample_peak(ebur128::channel{1}) == 0.0);
    try {
      s.prev_sample_peak(ebur128::channel{2});
      assert(false);
    } catch (const ebur128::invalid_channel_index_error&) {
    }

    const std::vector<double> first{0.25, 0.5, -0.75, -0.125};
    s.add_frames(ebur128::audio_data_view(first, ebur128::channel{2}));
    assert(s.prev_sample_peak(ebur128::channel{0}) == 0.75);
    assert(s.prev_sample_peak(ebur128::channel{1}) == 0.5);
    assert(s.sample_peak(ebur128::channel{0}) == 0.75);
    assert(s.sample_peak(ebur128::channel{1}) == 0.5);

    const std::vector<double> second{0.125, 0.25, -0.0625, -0.375};
    s.add_frames(ebur128::audio_data_view(second, ebur128::channel{2}));
    assert(s.prev_sample_peak(ebur128::channel{0}) == 0.125);
    assert(s.prev_sample_peak(ebur128::channel{1}) == 0.375);
    assert(s.sample_peak(ebur128::channel{0}) == 0.75);
    assert(s.sample_peak(ebur128::channel{1}) == 0.5);
  }

  return true;
}

static bool test_prev_true_peak() {
  {
    ebur128::state s(ebur128::channel{2}, ebur128::samplerate{48000},
                     ebur128::mode::m);
    try {
      s.prev_true_peak(ebur128::channel{0});
      assert(false);
    } catch (const ebur128::invalid_mode_error&) {
    }
  }

  {
    ebur128::state s(ebur128::channel{2}, ebur128::samplerate{48000},
                     ebur128::mode::true_peak);
    assert(s.prev_true_peak(ebur128::channel{0}) == 0.0);
    assert(s.prev_true_peak(ebur128::channel{1}) == 0.0);
    try {
      s.prev_true_peak(ebur128::channel{2});
      assert(false);
    } catch (const ebur128::invalid_channel_index_error&) {
    }
  }

  {
    constexpr std::size_t samplerate = 48000;
    constexpr std::size_t frames = 4800;
    constexpr std::size_t flush_frames = 64;
    constexpr double sample_amplitude = std::numbers::sqrt2 / 4.0;

    std::vector<double> loud(frames);
    std::vector<double> quiet(frames);
    for (std::size_t i = 0; i < frames; ++i) {
      const double sign = i % 4 < 2 ? 1.0 : -1.0;
      loud[i] = sign * sample_amplitude;
      quiet[i] = sign * sample_amplitude / 2.0;
    }
    const std::vector<double> flush(flush_frames);

    ebur128::state s(ebur128::channel{1}, ebur128::samplerate{samplerate},
                     ebur128::mode::true_peak);

    s.add_frames(ebur128::audio_data_view(loud, ebur128::channel{1}));
    const double loud_sample_peak = s.prev_sample_peak(ebur128::channel{0});
    const double loud_true_peak = s.prev_true_peak(ebur128::channel{0});
    assert(std::fabs(loud_sample_peak - sample_amplitude) <= 1e-12);
    assert(loud_true_peak > loud_sample_peak);
    assert(s.true_peak(ebur128::channel{0}) >= loud_true_peak);

    s.add_frames(ebur128::audio_data_view(flush, ebur128::channel{1}));
    s.add_frames(ebur128::audio_data_view(quiet, ebur128::channel{1}));
    const double quiet_sample_peak = s.prev_sample_peak(ebur128::channel{0});
    const double quiet_true_peak = s.prev_true_peak(ebur128::channel{0});
    assert(std::fabs(quiet_sample_peak - sample_amplitude / 2.0) <= 1e-12);
    assert(quiet_true_peak > quiet_sample_peak);
    assert(quiet_true_peak < loud_true_peak);
    assert(quiet_true_peak < s.true_peak(ebur128::channel{0}));
  }

  return true;
}

int main(int argc, char** argv) {
  const auto run_test = [&](std::string_view name, bool (*test)()) {
    std::string test_name(name);
    std::ranges::replace(test_name, '<', '_');
    std::ranges::replace(test_name, ',', '_');
    std::erase(test_name, '>');
    std::erase(test_name, ' ');

    if (argc == 2 && std::strcmp(argv[1], "--list-tests") == 0)
      std::cout << test_name << '\n';
    else if (argc == 1 ||
             (argc == 3 && std::strcmp(argv[1], "--run-test") == 0 &&
              test_name == argv[2]))
      assert(test());
  };
#define register_test(...) run_test(&#__VA_ARGS__[5], (__VA_ARGS__))

  constexpr bool plain = false;
  constexpr bool histogram = true;

#define register_test_all_types(name)                                          \
  register_test(name<short, plain>);                                           \
  register_test(name<short, histogram>);                                       \
  register_test(name<int, plain>);                                             \
  register_test(name<int, histogram>);                                         \
  register_test(name<float, plain>);                                           \
  register_test(name<float, histogram>);                                       \
  register_test(name<double, plain>);                                          \
  register_test(name<double, histogram>)

  register_test(test_simple);
  register_test(test_threaded_init);
#ifdef EBUR128_TEST_PERFORMANCE
  register_test(test_ftz_performance);
#endif
  register_test(test_init);
  register_test(test_set_channel);
  register_test(test_dual_mono);
  register_test(test_surround_channels);
  register_test(test_loudness_window);
  register_test(test_loudness_window_overflow);
  register_test(test_set_max_window);
  register_test(test_set_max_window_large_allocation);
  register_test(test_set_max_history);
  register_test(test_integer_overflow);
  register_test(test_version);
  register_test(test_change_parameters);
  register_test(test_global_loudness);
  register_test(test_global_loudness_quiet);
  register_test(test_global_loudness_multiple);
  register_test(test_loudness_range_quiet);
  register_test(test_loudness_range_multiple);
  register_test(test_loudness_range_partial_frames);
  register_test(test_relative_threshold);
  register_test(test_momentary_loudness);
  register_test(test_shortterm_loudness);
  register_test(test_true_peak);
  register_test(test_sample_peak);
  register_test(test_prev_true_peak);
  register_test(test_prev_sample_peak);
  register_test_all_types(test_signal_1);
  register_test_all_types(test_signal_2);
  register_test_all_types(test_signal_3);
  register_test_all_types(test_signal_4);
  register_test_all_types(test_signal_5);
  register_test_all_types(test_signal_6);
  register_test_all_types(test_signal_6_51);
  register_test_all_types(test_signal_9);
  register_test_all_types(test_signal_10);
  register_test_all_types(test_signal_11);
  register_test_all_types(test_signal_12);
  register_test_all_types(test_signal_13);
  register_test_all_types(test_signal_14);
  register_test_all_types(test_lra_1);
  register_test_all_types(test_lra_2);
  register_test_all_types(test_lra_3);
  register_test_all_types(test_lra_4);

#undef register_test_all_types
#undef register_test
}

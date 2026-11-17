#include <ebur128.h>
#include <ebur128.test.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <limits>
#include <numbers>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ebur128 {
using namespace ebur128::test;
}

namespace {

using clock_type = std::chrono::steady_clock;

struct options {
  unsigned long rate = 48000;
  unsigned int channels = 2;
  double seconds = 10.0;
  unsigned long chunk_ms = 100;
  unsigned int repeat = 3;
  std::string sample_type = "double";
};

enum class getter {
  none,
  momentary,
  shortterm,
};

struct scenario {
  const char* name;
  ebur128::mode mode;
  getter poll;
};

const scenario scenarios[] = {
    {"m_add", ebur128::mode::m, getter::none},
    {"m_poll", ebur128::mode::m, getter::momentary},
    {"s_poll", ebur128::mode::s, getter::shortterm},
    {"i_add", ebur128::mode::i, getter::none},
    {"i_lra_add", ebur128::mode::i | ebur128::mode::lra, getter::none},
    {"i_lra_hist_add",
     ebur128::mode::i | ebur128::mode::lra | ebur128::mode::histogram,
     getter::none},
    {"tp_add", ebur128::mode::true_peak, getter::none},
    {"i_tp_add", ebur128::mode::i | ebur128::mode::true_peak, getter::none},
    {"i_lra_tp_add",
     ebur128::mode::i | ebur128::mode::lra | ebur128::mode::true_peak,
     getter::none},
    {"i_lra_tp_hist_add",
     ebur128::mode::i | ebur128::mode::lra | ebur128::mode::true_peak |
         ebur128::mode::histogram,
     getter::none},
};

[[noreturn]] void fail(const char* message) {
  std::fprintf(stderr, "ebur128-benchmark: %s\n", message);
  std::exit(EXIT_FAILURE);
}

unsigned long parse_unsigned(const char* text, const char* name) {
  char* end = nullptr;
  const unsigned long value = std::strtoul(text, &end, 10);
  if (end == text || *end != '\0') {
    std::fprintf(stderr, "ebur128-benchmark: invalid %s: %s\n", name, text);
    std::exit(EXIT_FAILURE);
  }
  return value;
}

double parse_double(const char* text, const char* name) {
  char* end = nullptr;
  const double value = std::strtod(text, &end);
  if (end == text || *end != '\0' || !std::isfinite(value)) {
    std::fprintf(stderr, "ebur128-benchmark: invalid %s: %s\n", name, text);
    std::exit(EXIT_FAILURE);
  }
  return value;
}

const char* next_argument(int& index, int argc, char** argv) {
  ++index;
  if (index >= argc) {
    fail("missing option value");
  }
  return argv[index];
}

void print_usage(const char* program) {
  std::printf(
      "Usage: %s [options]\n"
      "\n"
      "Options:\n"
      "  --rate N          Sample rate in Hz (default: 48000)\n"
      "  --channels N      Channel count, 1-64 (default: 2)\n"
      "  --seconds S       Signal duration in seconds (default: 10)\n"
      "  --chunk-ms N      Frames added per call in ms (default: 100)\n"
      "  --repeat N        Timing repetitions; median is reported (default: "
      "3)\n"
      "  --sample-type T   short, int, float, or double (default: double)\n"
      "  --help            Show this help\n",
      program);
}

::channel channel_for(unsigned int channel) {
  static const ::channel channels[] = {
      EBUR128_LEFT,          EBUR128_RIGHT,          EBUR128_CENTER,
      EBUR128_LEFT_SURROUND, EBUR128_RIGHT_SURROUND,
  };
  return channels[channel % (sizeof(channels) / sizeof(channels[0]))];
}

void configure_channel_map(ebur128::state& state, unsigned int channels) {
  for (unsigned int channel = 0; channel < channels; ++channel) {
    state.set_channel(ebur128::channel{channel}, channel_for(channel));
  }
}

template <class T> T to_sample(double value);

template <> short to_sample<short>(double value) {
  return static_cast<short>(
      std::lround(std::clamp(value, -1.0, 1.0) * 32767.0));
}

template <> int to_sample<int>(double value) {
  return static_cast<int>(
      std::lround(std::clamp(value, -1.0, 1.0) * 2147483647.0));
}

template <> float to_sample<float>(double value) {
  return static_cast<float>(value);
}

template <> double to_sample<double>(double value) { return value; }

template <class T>
std::vector<T>
make_signal(std::size_t frames, unsigned int channels, unsigned long rate) {
  std::vector<T> signal(frames * channels);
  for (std::size_t frame = 0; frame < frames; ++frame) {
    const double time = static_cast<double>(frame) / static_cast<double>(rate);
    for (unsigned int channel = 0; channel < channels; ++channel) {
      const double frequency = 220.0 + 110.0 * static_cast<double>(channel);
      const double value =
          0.1 * std::sin(2.0 * std::numbers::pi * frequency * time) +
          0.01 * std::sin(2.0 * std::numbers::pi * 3.0 * frequency * time);
      signal[frame * channels + channel] = to_sample<T>(value);
    }
  }
  return signal;
}

template <class T>
void add_frames(ebur128::state& state,
                std::span<const T> data,
                ebur128::channel channels) {
  state.add_frames(ebur128::audio_data_view<T>(data, channels));
}

double read_getter(const ebur128::state& state, getter which) {
  switch (which) {
    case getter::none: return 0.0;
    case getter::momentary: return state.loudness_momentary();
    case getter::shortterm: return state.loudness_shortterm();
  }
  return 0.0;
}

template <class T>
double run_scenario(const scenario& current,
                    const options& opts,
                    std::span<const T> signal,
                    std::size_t frames,
                    std::size_t chunk_frames,
                    double& checksum) {
  std::vector<double> timings;
  timings.reserve(opts.repeat);

  for (unsigned int repetition = 0; repetition < opts.repeat; ++repetition) {
    ebur128::state state(ebur128::channel{opts.channels},
                         ebur128::samplerate{opts.rate}, current.mode);
    configure_channel_map(state, opts.channels);

    const auto start = clock_type::now();
    for (std::size_t first = 0; first < frames; first += chunk_frames) {
      const std::size_t count = std::min(chunk_frames, frames - first);
      add_frames(state,
                 signal.subspan(first * opts.channels, count * opts.channels),
                 ebur128::channel{opts.channels});
      checksum += read_getter(state, current.poll);
    }
    const auto end = clock_type::now();

    timings.push_back(std::chrono::duration<double>(end - start).count());
  }

  std::sort(timings.begin(), timings.end());
  return timings[timings.size() / 2];
}

template <class T> void run_all(const options& opts) {
  const std::size_t frames = static_cast<std::size_t>(
      std::llround(opts.seconds * static_cast<double>(opts.rate)));
  if (frames == 0) {
    fail("signal duration produces zero frames");
  }

  const std::size_t chunk_frames = std::max<std::size_t>(
      1, static_cast<std::size_t>(
             std::llround(static_cast<double>(opts.rate) *
                          static_cast<double>(opts.chunk_ms) / 1000.0)));

  const std::vector<T> signal =
      make_signal<T>(frames, opts.channels, opts.rate);

  std::printf(
      "sample=%s rate=%lu channels=%u frames=%zu chunk=%.3fms repeat=%u\n",
      opts.sample_type.c_str(), opts.rate, opts.channels, frames,
      static_cast<double>(chunk_frames) * 1000.0 /
          static_cast<double>(opts.rate),
      opts.repeat);
  std::printf("%-22s %12s %14s %12s\n", "scenario", "wall [ms]", "ns/frame",
              "realtime");

  double checksum = 0.0;
  const double duration =
      static_cast<double>(frames) / static_cast<double>(opts.rate);
  for (const scenario& current : scenarios) {
    const double seconds =
        run_scenario(current, opts, std::span<const T>(signal), frames,
                     chunk_frames, checksum);
    std::printf("%-22s %12.3f %14.1f %11.2fx\n", current.name, seconds * 1000.0,
                seconds * 1e9 / static_cast<double>(frames),
                duration / seconds);
  }
  std::printf("checksum=%.6f\n", checksum);
}

} // namespace

int main(int argc, char** argv) {
  options opts;

  for (int index = 1; index < argc; ++index) {
    const std::string_view argument = argv[index];
    if (argument == "--help") {
      print_usage(argv[0]);
      return EXIT_SUCCESS;
    }
    if (argument == "--rate") {
      opts.rate =
          parse_unsigned(next_argument(index, argc, argv), "sample rate");
    } else if (argument == "--channels") {
      opts.channels = static_cast<unsigned int>(
          parse_unsigned(next_argument(index, argc, argv), "channel count"));
    } else if (argument == "--seconds") {
      opts.seconds = parse_double(next_argument(index, argc, argv), "duration");
    } else if (argument == "--chunk-ms") {
      opts.chunk_ms =
          parse_unsigned(next_argument(index, argc, argv), "chunk size");
    } else if (argument == "--repeat") {
      opts.repeat = static_cast<unsigned int>(
          parse_unsigned(next_argument(index, argc, argv), "repeat count"));
    } else if (argument == "--sample-type") {
      opts.sample_type = next_argument(index, argc, argv);
    } else {
      std::fprintf(stderr, "ebur128-benchmark: unknown option: %s\n",
                   argv[index]);
      print_usage(argv[0]);
      return EXIT_FAILURE;
    }
  }

  if (opts.channels == 0 || opts.channels > 64) {
    fail("channel count must be between 1 and 64");
  }
  if (opts.rate < 16 || opts.rate > 2822400) {
    fail("sample rate must be between 16 and 2822400");
  }
  if (opts.seconds <= 0.0) {
    fail("duration must be positive");
  }
  if (opts.chunk_ms == 0) {
    fail("chunk size must be positive");
  }
  if (opts.repeat == 0) {
    fail("repeat count must be positive");
  }

  try {
    if (opts.sample_type == "short") {
      run_all<short>(opts);
    } else if (opts.sample_type == "int") {
      run_all<int>(opts);
    } else if (opts.sample_type == "float") {
      run_all<float>(opts);
    } else if (opts.sample_type == "double") {
      run_all<double>(opts);
    } else {
      fail("sample type must be short, int, float, or double");
    }
  } catch (const std::exception& error) {
    std::fprintf(stderr, "ebur128-benchmark: %s\n", error.what());
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

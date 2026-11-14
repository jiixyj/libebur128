#undef NDEBUG
#include <cassert>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <ranges>

#include <ebur128.h>
#include <ebur128.test.h>
#include <ebur128.test_signals.h>

namespace ebur128 {
using namespace ebur128::test;
}

static bool true_peak(const std::filesystem::path& path, double expected_peak) {
  {
    ebur128::test_signals::wav_file file(
        std::filesystem::path("ebu-loudness-test-setv05") / path);
    assert(file.channels() == 2);
    assert(file.bits() == 24);

    ebur128::state s(ebur128::channel{file.channels()},
                     ebur128::samplerate{file.samplerate()},
                     ebur128::mode::true_peak);

    s.add_frames(ebur128::audio_data_view(file.samples_32(),
                                          ebur128::channel{file.channels()}));

    double true_peak =
        std::ranges::max(std::views::iota(0u, file.channels()) |
                         std::views::transform([&](unsigned int channel) {
                           return s.true_peak(ebur128::channel{channel});
                         }));

    true_peak = 20 * std::log10(true_peak);
    assert(true_peak <= expected_peak + 0.2);
    assert(true_peak >= expected_peak - 0.4);
  }

  return true;
}

static bool test_true_peak_15() {
  return ::true_peak(std::filesystem::path("seq-3341-15-24bit.wav.wav"), -6.0);
}

static bool test_true_peak_16() {
  return ::true_peak(std::filesystem::path("seq-3341-16-24bit.wav.wav"), -6.0);
}

static bool test_true_peak_17() {
  return ::true_peak(std::filesystem::path("seq-3341-17-24bit.wav.wav"), -6.0);
}

static bool test_true_peak_18() {
  return ::true_peak(std::filesystem::path("seq-3341-18-24bit.wav.wav"), -6.0);
}

static bool test_true_peak_19() {
  return ::true_peak(std::filesystem::path("seq-3341-19-24bit.wav.wav"), 3.0);
}

static bool test_true_peak_20() {
  return ::true_peak(std::filesystem::path("seq-3341-20-24bit.wav.wav"), 0.0);
}

static bool test_true_peak_21() {
  return ::true_peak(std::filesystem::path("seq-3341-21-24bit.wav.wav"), 0.0);
}

static bool test_true_peak_22() {
  return ::true_peak(std::filesystem::path("seq-3341-22-24bit.wav.wav"), 0.0);
}

static bool test_true_peak_23() {
  return ::true_peak(std::filesystem::path("seq-3341-23-24bit.wav.wav"), 0.0);
}

int main(int argc, char** argv) {
  const auto run_test = [&](std::string_view test_name, bool (*test)()) {
    if (argc == 2 && std::strcmp(argv[1], "--list-tests") == 0)
      std::cout << test_name << '\n';
    else if (argc == 1 ||
             (argc == 3 && std::strcmp(argv[1], "--run-test") == 0 &&
              test_name == argv[2]))
      assert(test());
  };
#define register_test(...) run_test(&#__VA_ARGS__[5], (__VA_ARGS__))

  try {
    register_test(test_true_peak_15);
    register_test(test_true_peak_16);
    register_test(test_true_peak_17);
    register_test(test_true_peak_18);
    register_test(test_true_peak_19);
    register_test(test_true_peak_20);
    register_test(test_true_peak_21);
    register_test(test_true_peak_22);
    register_test(test_true_peak_23);
  } catch (const ebur128::test_signals::file_not_found&) {
    return 77;
  }

#undef register_test
}

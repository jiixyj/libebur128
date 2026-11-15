#undef NDEBUG
#include <cassert>

#include <algorithm>
#include <cmath>
#include <ranges>

#include <ebur128.h>
#include <ebur128.test.h>
#include <ebur128.test_signals.h>

namespace ebur128 {
using namespace ebur128::test;
}

static ebur128::test_signals::wav_file
open_file(const std::filesystem::path& path) {
  try {
    return ebur128::test_signals::wav_file(
        std::filesystem::path("ebu-loudness-test-setv05") / path);
  } catch (const ebur128::test_signals::file_not_found&) {
    throw ebur128::test::skipped{};
  }
}

static bool true_peak(const std::filesystem::path& path, double expected_peak) {
  {
    auto file = ::open_file(path);
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
  ebur128::test::suite suite(argc, argv);
  EBUR128_TEST_REGISTER(suite, test_true_peak_15);
  EBUR128_TEST_REGISTER(suite, test_true_peak_16);
  EBUR128_TEST_REGISTER(suite, test_true_peak_17);
  EBUR128_TEST_REGISTER(suite, test_true_peak_18);
  EBUR128_TEST_REGISTER(suite, test_true_peak_19);
  EBUR128_TEST_REGISTER(suite, test_true_peak_20);
  EBUR128_TEST_REGISTER(suite, test_true_peak_21);
  EBUR128_TEST_REGISTER(suite, test_true_peak_22);
  EBUR128_TEST_REGISTER(suite, test_true_peak_23);
  return suite.run();
}

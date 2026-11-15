#undef NDEBUG
#include <cassert>

#include <array>
#include <cmath>
#include <filesystem>
#include <utility>
#include <vector>

#include <ebur128.h>
#include <ebur128.test.h>
#include <ebur128.test_signals.h>

namespace ebur128 {
using namespace ebur128::test;
}

namespace {

constexpr double global_loudness_tolerance = 0.1;
constexpr double loudness_range_tolerance = 1.0;
constexpr double multiple_tolerance = 0.05;

std::filesystem::path test_set_file(const char* filename) {
  return std::filesystem::path("ebu-loudness-test-setv05") / filename;
}

void configure_channels(ebur128::state& s, unsigned int channels) {
  if (channels != 5)
    return;

  s.set_channel(ebur128::channel{0}, EBUR128_LEFT);
  s.set_channel(ebur128::channel{1}, EBUR128_RIGHT);
  s.set_channel(ebur128::channel{2}, EBUR128_CENTER);
  s.set_channel(ebur128::channel{3}, EBUR128_LEFT_SURROUND);
  s.set_channel(ebur128::channel{4}, EBUR128_RIGHT_SURROUND);
}

void add_frames(ebur128::state& s,
                const ebur128::test_signals::wav_file& file) {
  if (file.bits() == 16) {
    s.add_frames(ebur128::audio_data_view(file.samples_16(),
                                          ebur128::channel{file.channels()}));
  } else {
    s.add_frames(ebur128::audio_data_view(file.samples_32(),
                                          ebur128::channel{file.channels()}));
  }
}

template <ebur128::test_signals::sample_type T>
void add_frames(ebur128::state& s,
                const ebur128::test_signals::test_signal<T>& signal) {
  s.add_frames(ebur128::audio_data_view(signal.samples(),
                                        ebur128::channel{signal.channels()}));
}

ebur128::state make_file_state(const char* filename, ebur128::mode mode) {
  try {
    ebur128::test_signals::wav_file file(::test_set_file(filename));
    ebur128::state s(ebur128::channel{file.channels()},
                     ebur128::samplerate{file.samplerate()}, mode);
    ::configure_channels(s, file.channels());
    ::add_frames(s, file);
    return s;
  } catch (const ebur128::test_signals::file_not_found&) {
    throw ebur128::test::skipped{};
  }
}

double file_global_loudness(const char* filename) {
  auto s = ::make_file_state(filename, ebur128::mode::i);
  return s.loudness_global();
}

double file_loudness_range(const char* filename) {
  auto s = ::make_file_state(filename, ebur128::mode::lra);
  return s.loudness_range();
}

bool test_global_loudness_6_6channels() {
  const double result =
      ::file_global_loudness("seq-3341-6-6channels-WAVEEX-16bit.wav");
  assert(std::fabs(result - -23.0) <= global_loudness_tolerance);
  return true;
}

bool test_global_loudness_7() {
  const double result =
      ::file_global_loudness("seq-3341-7_seq-3342-5-24bit.wav");
  assert(std::fabs(result - -23.0) <= global_loudness_tolerance);
  return true;
}

bool test_global_loudness_8() {
  const double result =
      ::file_global_loudness("seq-3341-2011-8_seq-3342-6-24bit-v02.wav");
  assert(std::fabs(result - -23.0) <= global_loudness_tolerance);
  return true;
}

bool test_loudness_range_7() {
  const double result =
      ::file_loudness_range("seq-3341-7_seq-3342-5-24bit.wav");
  assert(std::fabs(result - 5.0) <= loudness_range_tolerance);
  return true;
}

bool test_loudness_range_8() {
  const double result =
      ::file_loudness_range("seq-3341-2011-8_seq-3342-6-24bit-v02.wav");
  assert(std::fabs(result - 15.0) <= loudness_range_tolerance);
  return true;
}

// Regression test for an old bug where `loudness_global_multiple` calculated
// the relative threshold only from the last file in the array.
bool test_global_loudness_multiple() {
  std::vector<ebur128::state> states = {};
  states.reserve(6);
  states.push_back(::make_file_state("seq-3341-2011-8_seq-3342-6-24bit-v02.wav",
                                     ebur128::mode::i));

  for (const int number : std::array{2, 3, 4, 5, 1}) {
    const ebur128::test_signals::test_signal<double> signal(
        ebur128::test_signals::test_signal_kind::ebu3341, number);
    ebur128::state s(ebur128::channel{signal.channels()},
                     ebur128::samplerate{signal.samplerate()},
                     ebur128::mode::i);
    ::configure_channels(s, signal.channels());
    ::add_frames(s, signal);
    states.push_back(std::move(s));
  }

  const double result = ebur128::state::loudness_global_multiple(states);
  assert(result >= -23.18758 - multiple_tolerance);
  assert(result <= -23.18758 + multiple_tolerance);
  return true;
}

} // namespace

int main(int argc, char** argv) {
  ebur128::test::suite suite(argc, argv);
  EBUR128_TEST_REGISTER(suite, test_global_loudness_6_6channels);
  EBUR128_TEST_REGISTER(suite, test_global_loudness_7);
  EBUR128_TEST_REGISTER(suite, test_global_loudness_8);
  EBUR128_TEST_REGISTER(suite, test_loudness_range_7);
  EBUR128_TEST_REGISTER(suite, test_loudness_range_8);
  EBUR128_TEST_REGISTER(suite, test_global_loudness_multiple);
  return suite.run();
}

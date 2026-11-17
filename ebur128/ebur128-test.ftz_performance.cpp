#include <ebur128.h>
#include <ebur128.test.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <vector>

namespace ebur128 {
using namespace ebur128::test;
}

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

int main(int argc, char** argv) {
  ebur128::test::suite suite(argc, argv);
  EBUR128_TEST_REGISTER(suite, test_ftz_performance);
  return suite.run();
}

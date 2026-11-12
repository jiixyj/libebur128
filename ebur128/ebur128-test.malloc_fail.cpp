#undef NDEBUG
#include <cassert>

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

#include <ebur128.h>

namespace {

std::size_t allocation_count = 0;
std::size_t fail_at = std::numeric_limits<std::size_t>::max();
bool injected_failure = false;

std::vector<double> make_input(unsigned int channels,
                               unsigned long samplerate,
                               std::size_t seconds) {
  constexpr double pi = 3.14159265358979323846;
  const std::size_t frames = static_cast<std::size_t>(samplerate) * seconds;
  std::vector<double> input(frames * channels);
  for (std::size_t i = 0; i < frames; ++i) {
    const double sample =
        0.1 * std::sin(2.0 * pi * 1000.0 * static_cast<double>(i) /
                       static_cast<double>(samplerate));
    for (unsigned int c = 0; c < channels; ++c) {
      input[i * channels + c] = sample;
    }
  }
  return input;
}

void reset(std::size_t index) {
  allocation_count = 0;
  fail_at = index;
  injected_failure = false;
}

bool check_result(int result) {
  if (result == EBUR128_SUCCESS || result == EBUR128_ERROR_NO_CHANGE) {
    return true;
  }
  assert(injected_failure);
  return false;
}

void run_workload(bool histogram,
                  const std::vector<double>& stereo_input,
                  const std::vector<double>& quad_input) {
  int mode = EBUR128_MODE_M | EBUR128_MODE_S | EBUR128_MODE_I |
             EBUR128_MODE_LRA | EBUR128_MODE_TRUE_PEAK;
  if (histogram) {
    mode |= EBUR128_MODE_HISTOGRAM;
  }

  ebur128_state* state = ebur128_init(2, 48000, mode);
  if (!state) {
    assert(injected_failure);
    return;
  }

  int result = ebur128_add_frames_double(state, stereo_input.data(),
                                         stereo_input.size() / 2);
  if (!check_result(result) || injected_failure) {
    ebur128_destroy(&state);
    return;
  }

  double out = 0.0;
  result = ebur128_loudness_global(state, &out);
  if (!check_result(result) || injected_failure) {
    ebur128_destroy(&state);
    return;
  }

  result = ebur128_loudness_range(state, &out);
  if (!check_result(result) || injected_failure) {
    ebur128_destroy(&state);
    return;
  }

  result = ebur128_set_max_window(state, 5000);
  if (!check_result(result) || injected_failure) {
    ebur128_destroy(&state);
    return;
  }

  result = ebur128_change_parameters(state, 4, 96000);
  if (!check_result(result) || injected_failure) {
    ebur128_destroy(&state);
    return;
  }

  result = ebur128_add_frames_double(state, quad_input.data(),
                                     quad_input.size() / 4);
  (void) check_result(result);

  ebur128_destroy(&state);
}

void run_config(bool histogram,
                const std::vector<double>& stereo_input,
                const std::vector<double>& quad_input) {
  reset(std::numeric_limits<std::size_t>::max());
  run_workload(histogram, stereo_input, quad_input);
  assert(!injected_failure);
  const std::size_t total_allocations = allocation_count;

  for (std::size_t i = 0; i <= total_allocations; ++i) {
    std::fprintf(stderr, "%d/%d\r", (int) i, (int) total_allocations);

    reset(i);
    run_workload(histogram, stereo_input, quad_input);
    if (!injected_failure) {
      assert(i == total_allocations);
      return;
    }
    assert(i < total_allocations);
  }
  assert(false);
}

} // namespace

extern "C" void* ebur128_test_malloc(std::size_t size) {
  if (allocation_count++ == fail_at) {
    injected_failure = true;
    return nullptr;
  }
  return std::malloc(size);
}

extern "C" void* ebur128_test_calloc(std::size_t nmemb, std::size_t size) {
  if (allocation_count++ == fail_at) {
    injected_failure = true;
    return nullptr;
  }
  return std::calloc(nmemb, size);
}

int main() {
  const std::vector<double> stereo_input = make_input(2, 48000, 5);
  const std::vector<double> quad_input = make_input(4, 96000, 1);
  run_config(false, stereo_input, quad_input);
  run_config(true, stereo_input, quad_input);
}

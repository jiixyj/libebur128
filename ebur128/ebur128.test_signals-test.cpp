#include <ebur128.test_signals.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using test_signal = ebur128::test_signals::test_signal<double>;

constexpr int skip_code = 77;
constexpr double max_full_scale_difference = 0.001;
constexpr unsigned long expected_samplerate = 48000;

struct reference_file {
  int number;
  int variant;
  unsigned int bits;
  std::string filename;
  bool synthesized;
  bool lra = false;
};

std::vector<reference_file> make_reference_files() {
  std::vector<reference_file> files = {};
  files.push_back({1, 0, 16, "seq-3341-1-16bit.wav", true});
  files.push_back({2, 0, 16, "seq-3341-2-16bit.wav", true});
  files.push_back({3, 0, 16, "seq-3341-3-16bit-v02.wav", true});
  files.push_back({4, 0, 16, "seq-3341-4-16bit-v02.wav", true});
  files.push_back({5, 0, 16, "seq-3341-5-16bit-v02.wav", true});
  files.push_back({6, 1, 16, "seq-3341-6-5channels-16bit.wav", true});
  files.push_back({7, 0, 24, "seq-3341-7_seq-3342-5-24bit.wav", false});
  files.push_back(
      {8, 0, 24, "seq-3341-2011-8_seq-3342-6-24bit-v02.wav", false});
  files.push_back({9, 0, 24, "seq-3341-9-24bit.wav", true});

  for (int i = 1; i <= 20; ++i) {
    files.push_back(
        {10, i, 24, "seq-3341-10-" + std::to_string(i) + "-24bit.wav", true});
  }

  files.push_back({11, 0, 24, "seq-3341-11-24bit.wav", true});
  files.push_back({12, 0, 24, "seq-3341-12-24bit.wav", true});

  for (int i = 1; i <= 20; ++i) {
    const auto suffix = i <= 2 ? "-24bit.wav" : "-24bit.wav.wav";
    files.push_back(
        {13, i, 24, "seq-3341-13-" + std::to_string(i) + suffix, true});
  }

  files.push_back({14, 0, 24, "seq-3341-14-24bit.wav.wav", true});
  files.push_back({1, 0, 16, "seq-3342-1-16bit.wav", true, true});
  files.push_back({2, 0, 16, "seq-3342-2-16bit.wav", true, true});
  files.push_back({3, 0, 16, "seq-3342-3-16bit.wav", true, true});
  files.push_back({4, 0, 16, "seq-3342-4-16bit.wav", true, true});
  return files;
}

std::int32_t quantize(double sample, unsigned int bits) {
  const auto scale = 1LL << (bits - 1);
  auto rounded = std::llround(sample * static_cast<double>(scale));
  rounded = std::clamp(rounded, -scale, scale - 1);
  return static_cast<std::int32_t>(rounded);
}

long long max_difference(unsigned int bits) {
  return static_cast<long long>(static_cast<double>(1LL << (bits - 1)) *
                                max_full_scale_difference);
}

void compare_signal(const test_signal& expected,
                    unsigned int bits,
                    const std::filesystem::path& path) {
  const ebur128::test_signals::wav_file actual(path);

  const auto expected_frames = expected.frames();
  const auto expected_channels = expected.channels();
  const auto expected_samples = expected.samples();
  const auto actual_frames = actual.frames();

  if (actual.samplerate() != expected.samplerate() ||
      actual.channels() != expected_channels || actual.bits() != bits ||
      actual_frames != expected_frames) {
    throw std::runtime_error(
        "unexpected sample rate, channel count, bit depth, or length");
  }

  const auto tolerance = max_difference(bits);
  std::size_t first_mismatch = expected_frames;
  long long maximum_difference = 0;

  for (std::size_t frame = 0; frame < expected_frames; ++frame) {
    for (unsigned int channel = 0; channel < expected_channels; ++channel) {
      const auto index = frame * expected_channels + channel;
      const auto expected_sample = quantize(expected_samples[index], bits);
      auto actual_sample = actual.sample_at(index);
      if (bits == 24)
        actual_sample >>= 8;
      else if (bits == 16)
        actual_sample >>= 16;
      const auto difference =
          std::llabs(static_cast<long long>(actual_sample) - expected_sample);
      maximum_difference = std::max(maximum_difference, difference);

      if (difference > tolerance && first_mismatch == expected_frames) {
        first_mismatch = frame;
        std::fprintf(stderr,
                     "%s: first mismatch at frame %zu, channel %u: "
                     "expected %d, actual %d, difference %lld LSB\n",
                     path.filename().string().c_str(), frame, channel,
                     static_cast<int>(expected_sample),
                     static_cast<int>(actual_sample),
                     static_cast<long long>(difference));
      }
    }
  }

  std::printf("%s: %zu frames, maximum difference %lld LSB\n",
              path.filename().string().c_str(), expected_frames,
              static_cast<long long>(maximum_difference));

  if (first_mismatch != expected_frames) {
    throw std::runtime_error("waveform does not match");
  }
}

void validate_authentic_signal(const reference_file& reference,
                               const std::filesystem::path& path) {
  const ebur128::test_signals::wav_file actual(path);
  const auto frames = actual.frames();

  if (actual.samplerate() != expected_samplerate || actual.channels() != 2 ||
      actual.bits() != reference.bits || frames == 0) {
    throw std::runtime_error("unexpected authentic signal format");
  }

  std::printf("%s: %zu frames, %u channels, %u bits (authentic)\n",
              path.filename().string().c_str(), frames, actual.channels(),
              actual.bits());
}

bool reference_file_exists(const std::filesystem::path& directory,
                           const reference_file& reference) {
  const auto path = directory / reference.filename;
  if (!std::filesystem::is_regular_file(path)) {
    std::fprintf(stderr, "skipping: missing reference file %s\n",
                 path.string().c_str());
    return false;
  }
  return true;
}

} // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::fprintf(stderr, "usage: %s <ebu-loudness-test-setv05 directory>\n",
                 argv[0]);
    return 2;
  }

  const std::filesystem::path directory = argv[1];
  const auto reference_files = ::make_reference_files();

  if (std::ranges::any_of(reference_files, [&](const auto& reference_file) {
        return !reference_file_exists(directory, reference_file);
      })) {
    return skip_code;
  }

  try {
    for (const auto& reference : reference_files) {
      const auto path = directory / reference.filename;
      try {
        if (reference.synthesized) {
          ::compare_signal(test_signal(reference.lra
                                           ? test_signal::kind::ebu3342
                                           : test_signal::kind::ebu3341,
                                       reference.number, reference.variant),
                           reference.bits, path);
        } else {
          ::validate_authentic_signal(reference, path);
        }
      } catch (const std::runtime_error& e) {
        throw std::runtime_error(path.string() + ": " + e.what());
      }
    }
  } catch (const std::exception& exception) {
    std::fprintf(stderr, "error: %s\n", exception.what());
    return 1;
  }

  return 0;
}

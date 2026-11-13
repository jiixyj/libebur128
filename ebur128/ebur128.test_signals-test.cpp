#include <ebur128.test_signals.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using ebur128::test_signals::test_signal;

constexpr int skip_code = 77;
constexpr double max_full_scale_difference = 0.001;
constexpr std::uint16_t wav_format_pcm = 1;
constexpr unsigned long expected_samplerate = 48000;

struct wav_file {
  unsigned long samplerate;
  unsigned int channels;
  unsigned int bits;
  std::vector<std::int32_t> samples;
};

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
  files.push_back({6, 0, 16, "seq-3341-6-5channels-16bit.wav", true});
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

std::runtime_error error(const std::filesystem::path& path,
                         const std::string& message) {
  return std::runtime_error(path.string() + ": " + message);
}

std::uint16_t read_u16(std::span<const std::uint8_t> data, std::size_t offset) {
  const auto low = static_cast<std::uint16_t>(data[offset]);
  const auto high = static_cast<std::uint16_t>(data[offset + 1]) << 8;
  return static_cast<std::uint16_t>(low | high);
}

std::uint32_t read_u32(std::span<const std::uint8_t> data, std::size_t offset) {
  const auto b0 = static_cast<std::uint32_t>(data[offset]);
  const auto b1 = static_cast<std::uint32_t>(data[offset + 1]) << 8;
  const auto b2 = static_cast<std::uint32_t>(data[offset + 2]) << 16;
  const auto b3 = static_cast<std::uint32_t>(data[offset + 3]) << 24;
  return b0 | b1 | b2 | b3;
}

std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
  const auto size = std::filesystem::file_size(path);
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    throw error(path, "cannot open file");
  }

  std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
  if (!data.empty() &&
      !stream.read(reinterpret_cast<char*>(data.data()),
                   static_cast<std::streamsize>(data.size()))) {
    throw error(path, "cannot read file");
  }
  return data;
}

wav_file parse_wav(std::span<const std::uint8_t> data,
                   const std::filesystem::path& path) {
  if (data.size() < 12 || std::memcmp(data.data(), "RIFF", 4) != 0 ||
      std::memcmp(data.data() + 8, "WAVE", 4) != 0) {
    throw error(path, "not a RIFF/WAVE file");
  }

  std::size_t format_offset = data.size();
  std::size_t format_size = 0;
  std::size_t sample_offset = data.size();
  std::size_t sample_size = 0;

  std::size_t offset = 12;
  while (offset + 8 <= data.size()) {
    const auto chunk_size = read_u32(data, offset + 4);
    const auto chunk_offset = offset + 8;
    if (chunk_size > data.size() - chunk_offset) {
      throw error(path, "truncated chunk");
    }

    if (std::memcmp(data.data() + offset, "fmt ", 4) == 0) {
      format_offset = chunk_offset;
      format_size = chunk_size;
    } else if (std::memcmp(data.data() + offset, "data", 4) == 0) {
      sample_offset = chunk_offset;
      sample_size = chunk_size;
    }

    offset = chunk_offset + chunk_size;
    if (offset % 2 != 0) {
      ++offset;
    }
  }

  if (format_offset == data.size() || format_size < 16) {
    throw error(path, "missing or invalid fmt chunk");
  }
  if (sample_offset == data.size()) {
    throw error(path, "missing data chunk");
  }

  const auto format = read_u16(data, format_offset);
  const auto channels = read_u16(data, format_offset + 2);
  const auto samplerate = read_u32(data, format_offset + 4);
  const auto block_align = read_u16(data, format_offset + 12);
  const auto bits = read_u16(data, format_offset + 14);
  const std::size_t bytes_per_sample = bits / 8;

  if (format != wav_format_pcm || channels == 0 || (bits != 16 && bits != 24) ||
      samplerate != expected_samplerate ||
      block_align != channels * bytes_per_sample) {
    throw error(path, "unsupported WAV format");
  }
  if (sample_size % block_align != 0) {
    throw error(path, "invalid data chunk size");
  }

  std::vector<std::int32_t> samples;
  samples.reserve(sample_size / bytes_per_sample);
  for (std::size_t i = 0; i < sample_size; i += bytes_per_sample) {
    if (bits == 16) {
      samples.push_back(
          std::bit_cast<std::int16_t>(read_u16(data, sample_offset + i)));
    } else {
      auto value =
          static_cast<std::uint32_t>(data[sample_offset + i]) |
          (static_cast<std::uint32_t>(data[sample_offset + i + 1]) << 8) |
          (static_cast<std::uint32_t>(data[sample_offset + i + 2]) << 16);
      if ((value & 0x800000U) != 0) {
        value |= 0xFF000000U;
      }
      samples.push_back(std::bit_cast<std::int32_t>(value));
    }
  }

  return wav_file{samplerate, channels, bits, std::move(samples)};
}

wav_file load_wav(const std::filesystem::path& path) {
  const auto data = read_file(path);
  return parse_wav(data, path);
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
  const auto actual = load_wav(path);

  const auto expected_frames = expected.frames();
  const auto expected_channels = expected.channels();
  const auto expected_samples = expected.samples();
  const auto actual_frames = actual.samples.size() / actual.channels;

  if (actual.samplerate != expected.samplerate() ||
      actual.channels != expected_channels || actual.bits != bits ||
      actual_frames != expected_frames) {
    throw error(path, "unexpected sample rate, channel count, bit depth, or "
                      "length");
  }

  const auto tolerance = max_difference(bits);
  std::size_t first_mismatch = expected_frames;
  long long maximum_difference = 0;

  for (std::size_t frame = 0; frame < expected_frames; ++frame) {
    for (unsigned int channel = 0; channel < expected_channels; ++channel) {
      const auto index = frame * expected_channels + channel;
      const auto expected_sample = quantize(expected_samples[index], bits);
      const auto actual_sample = actual.samples[index];
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
    throw error(path, "waveform does not match");
  }
}

void validate_authentic_signal(const reference_file& reference,
                               const std::filesystem::path& path) {
  const auto actual = load_wav(path);
  const auto frames = actual.samples.size() / actual.channels;

  if (actual.samplerate != expected_samplerate || actual.channels != 2 ||
      actual.bits != reference.bits || frames == 0) {
    throw error(path, "unexpected authentic signal format");
  }

  std::printf("%s: %zu frames, %u channels, %u bits (authentic)\n",
              path.filename().string().c_str(), frames, actual.channels,
              actual.bits);
}

bool all_reference_files_exist(
    const std::filesystem::path& directory,
    const std::vector<reference_file>& reference_files) {
  for (const auto& reference : reference_files) {
    std::error_code ec;
    const auto path = directory / reference.filename;
    if (!std::filesystem::is_regular_file(path, ec) || ec) {
      std::fprintf(stderr, "skipping: missing reference file %s\n",
                   path.string().c_str());
      return false;
    }
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

  if (!::all_reference_files_exist(directory, reference_files)) {
    return skip_code;
  }

  try {
    for (const auto& reference : reference_files) {
      const auto path = directory / reference.filename;
      if (reference.synthesized) {
        const auto kind = reference.lra ? test_signal::kind::ebu3342
                                        : test_signal::kind::ebu3341;
        const auto expected =
            test_signal{kind, reference.number, reference.variant};
        ::compare_signal(expected, reference.bits, path);
      } else {
        ::validate_authentic_signal(reference, path);
      }
    }
  } catch (const std::exception& exception) {
    std::fprintf(stderr, "error: %s\n", exception.what());
    return 1;
  }

  return 0;
}

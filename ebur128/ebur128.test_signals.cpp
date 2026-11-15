#include <ebur128.test_signals.h>

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ebur128::test_signals {
namespace {

constexpr std::uint16_t wav_format_pcm = 1;
constexpr std::uint16_t wav_format_extensible = 0xFFFE;

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
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    throw ebur128::test_signals::file_not_found("cannot open file");
  }

  return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(stream),
                                   std::istreambuf_iterator<char>());
}

//

struct segment {
  double seconds;
  std::vector<double> dbfs;
};

template <sample_type T> struct signal_data {
  unsigned long samplerate;
  unsigned int channels;
  std::vector<T> samples;
};

segment tone(double seconds, std::initializer_list<double> dbfs) {
  return segment{seconds, dbfs};
}

segment silence(double seconds) { return segment{seconds, {}}; }

std::vector<segment> segments_for(int number, int variant) {
  if (number == 6 && (variant < 1 || variant > 2)) {
    throw std::invalid_argument("invalid test signal variant");
  }
  if ((number == 10 || number == 13) && (variant < 1 || variant > 20)) {
    throw std::invalid_argument("invalid test signal variant");
  }
  if (number != 6 && number != 10 && number != 13 && variant != 0) {
    throw std::invalid_argument("invalid test signal variant");
  }

  switch (number) {
    case 1: return {tone(20.0, {-23.0})};
    case 2: return {tone(20.0, {-33.0})};
    case 3:
      return {tone(10.0, {-36.0}), tone(60.0, {-23.0}), tone(10.0, {-36.0})};
    case 4:
      return {tone(10.0, {-72.0}), tone(10.0, {-36.0}), tone(60.0, {-23.0}),
              tone(10.0, {-36.0}), tone(10.0, {-72.0})};
    case 5:
      return {tone(20.0, {-26.0}), tone(20.1, {-20.0}), tone(20.0, {-26.0})};
    case 6: return {tone(20.0, {-28.0, -28.0, -24.0, -30.0, -30.0})};
    case 7:
    case 8:
      throw std::invalid_argument("authentic programme cannot be generated");
    case 9: {
      std::vector<segment> result;
      for (int i = 0; i < 5; ++i) {
        result.push_back(tone(1.34, {-20.0}));
        result.push_back(tone(1.66, {-30.0}));
      }
      return result;
    }
    case 10:
      return {silence((variant - 1) * 0.15), tone(3.0, {-23.0}), silence(1.0)};
    case 11: {
      std::vector<segment> result;
      for (int i = 0; i < 20; ++i) {
        result.push_back(silence(i * 0.15));
        result.push_back(tone(3.0, {-38.0 + i}));
        result.push_back(silence(3.0 - i * 0.15));
      }
      return result;
    }
    case 12: {
      std::vector<segment> result;
      for (int i = 0; i < 25; ++i) {
        result.push_back(tone(0.18, {-20.0}));
        result.push_back(tone(0.22, {-30.0}));
      }
      return result;
    }
    case 13:
      return {silence((variant - 1) * 0.02), tone(0.4, {-23.0}), silence(1.0)};
    case 14: {
      std::vector<segment> result;
      for (int i = 0; i < 20; ++i) {
        result.push_back(silence(i * 0.02));
        result.push_back(tone(0.4, {-38.0 + i}));
        result.push_back(silence(0.4 - i * 0.02));
      }
      return result;
    }
    default: throw std::invalid_argument("unknown test signal");
  }
}

template <sample_type T> unsigned int channels_for(int number, int variant) {
  return number == 6
             ? (variant == 1   ? 5
                : variant == 2 ? 6
                               : throw std::invalid_argument("invalid variant"))
             : test_signal<T>::default_channels;
}

std::vector<segment> lra_segments_for(int number) {
  switch (number) {
    case 1: return {tone(20.0, {-20.0}), tone(20.0, {-30.0})};
    case 2: return {tone(20.0, {-20.0}), tone(20.0, {-15.0})};
    case 3: return {tone(20.0, {-40.0}), tone(20.0, {-20.0})};
    case 4:
      return {tone(20.0, {-50.0}), tone(20.0, {-35.0}), tone(20.0, {-20.0}),
              tone(20.0, {-35.0}), tone(20.0, {-50.0})};
    default: throw std::invalid_argument("unknown LRA test signal");
  }
}

template <sample_type T> std::size_t frames_in_segment(const segment& value) {
  return static_cast<std::size_t>(std::llround(
      value.seconds * static_cast<double>(test_signal<T>::default_samplerate)));
}

template <sample_type T> T convert_sample(double sample) {
  if constexpr (std::same_as<T, short> || std::same_as<T, int>) {
    constexpr auto min = static_cast<long long>(std::numeric_limits<T>::min());
    constexpr auto max = static_cast<long long>(std::numeric_limits<T>::max());
    const auto scale = static_cast<double>(-min);
    return static_cast<T>(std::clamp(std::llround(sample * scale), min, max));
  } else {
    return static_cast<T>(sample);
  }
}

template <sample_type T>
signal_data<T> make_signal(const std::vector<segment>& segments,
                           unsigned int channels) {
  bool has_lfe = false;
  if (channels == 6) {
    channels = 5;
    has_lfe = true;
  }

  std::size_t frames = 0;
  for (const auto& value : segments) {
    frames += frames_in_segment<T>(value);
  }

  signal_data<T> result{
      test_signal<T>::default_samplerate,
      channels + !!has_lfe,
      std::vector<T>(frames * (channels + !!has_lfe)),
  };

  std::size_t frame = 0;
  for (const auto& value : segments) {
    const auto segment_frames = frames_in_segment<T>(value);
    if (value.dbfs.empty()) {
      frame += segment_frames;
      continue;
    }
    if (value.dbfs.size() != 1 && value.dbfs.size() != channels) {
      throw std::logic_error("invalid channel count in test signal");
    }

    std::vector<double> amplitudes;
    amplitudes.reserve(channels);
    for (unsigned int channel = 0; channel < channels; ++channel) {
      const auto dbfs =
          value.dbfs.size() == 1 ? value.dbfs.front() : value.dbfs[channel];
      amplitudes.push_back(std::pow(10.0, dbfs / 20.0));
    }

    for (std::size_t i = 0; i < segment_frames; ++i, ++frame) {
      const double phase =
          2.0 * std::numbers::pi * test_signal<T>::default_frequency *
          static_cast<double>(frame) /
          static_cast<double>(test_signal<T>::default_samplerate);

      for (unsigned int channel = 0; channel < channels; ++channel) {
        unsigned int channel_tmp = channel;
        if (has_lfe && channel_tmp >= 3) {
          ++channel_tmp;
        }

        result.samples[frame * (channels + !!has_lfe) + channel_tmp] =
            convert_sample<T>(amplitudes[channel] * std::sin(phase));
      }
    }
  }

  return result;
}

} // namespace

wav_file::wav_file(std::span<const std::uint8_t> data) {
  if (data.size() < 12 || std::memcmp(data.data(), "RIFF", 4) != 0 ||
      std::memcmp(data.data() + 8, "WAVE", 4) != 0) {
    throw std::runtime_error("not a RIFF/WAVE file");
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
      throw std::runtime_error("truncated chunk");
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
    throw std::runtime_error("missing or invalid fmt chunk");
  }
  if (sample_offset == data.size()) {
    throw std::runtime_error("missing data chunk");
  }

  const auto format = read_u16(data, format_offset);
  const auto channels = read_u16(data, format_offset + 2);
  const auto samplerate = read_u32(data, format_offset + 4);
  const auto block_align = read_u16(data, format_offset + 12);
  const auto bits = read_u16(data, format_offset + 14);
  const std::size_t bytes_per_sample = bits / 8;
  const bool pcm = format == wav_format_pcm ||
                   (format == wav_format_extensible && format_size >= 40 &&
                    read_u16(data, format_offset + 18) == bits &&
                    read_u16(data, format_offset + 24) == wav_format_pcm);

  if (!pcm || channels == 0 || (bits != 16 && bits != 24) ||
      samplerate != 48000 || block_align != channels * bytes_per_sample) {
    throw std::runtime_error("unsupported WAV format");
  }
  if (sample_size % block_align != 0) {
    throw std::runtime_error("invalid data chunk size");
  }

  std::vector<std::int16_t> samples_16 = {};
  std::vector<std::int32_t> samples_24 = {};

  if (bits == 16) {
    samples_16.reserve(sample_size / bytes_per_sample);
  } else {
    samples_24.reserve(sample_size / bytes_per_sample);
  }

  for (std::size_t i = 0; i < sample_size; i += bytes_per_sample) {
    if (bits == 16) {
      samples_16.push_back(
          std::bit_cast<std::int16_t>(read_u16(data, sample_offset + i)));
    } else {
      auto value =
          static_cast<std::uint32_t>(data[sample_offset + i]) |
          (static_cast<std::uint32_t>(data[sample_offset + i + 1]) << 8) |
          (static_cast<std::uint32_t>(data[sample_offset + i + 2]) << 16);
      if ((value & 0x800000U) != 0) {
        value |= 0xFF000000U;
      }
      auto sample = std::bit_cast<std::int32_t>(value);
      sample <<= 8;
      samples_24.push_back(sample);
    }
  }

  samplerate_ = samplerate;
  channels_ = channels;
  bits_ = bits;
  if (bits == 16) {
    samples_ = std::move(samples_16);
  } else {
    samples_ = std::move(samples_24);
  }
}

wav_file::wav_file(const std::filesystem::path& path)
    : wav_file(read_file(path)) {}

template <sample_type T>
test_signal<T>::test_signal(test_signal_kind type, int number, int variant) {
  signal_data<T> data;

  switch (type) {
    case test_signal_kind::ebu3341:
      data = make_signal<T>(segments_for(number, variant),
                            channels_for<T>(number, variant));
      break;
    case test_signal_kind::ebu3342:
      if (variant != 0)
        throw std::invalid_argument("invalid test signal variant");
      data = make_signal<T>(lra_segments_for(number),
                            test_signal<T>::default_channels);
      break;
    default: throw std::invalid_argument("unknown test signal kind");
  }

  samplerate_ = data.samplerate;
  channels_ = data.channels;
  samples_ = std::move(data.samples);
}

template <sample_type T>
std::size_t test_signal<T>::frames_for_seconds(double seconds) const {
  return static_cast<std::size_t>(
      std::llround(seconds * static_cast<double>(samplerate_)));
}

template class test_signal<short>;
template class test_signal<int>;
template class test_signal<float>;
template class test_signal<double>;

} // namespace ebur128::test_signals

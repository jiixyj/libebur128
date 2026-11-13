#include <ebur128.test_signals.h>

#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <numbers>
#include <stdexcept>
#include <utility>
#include <vector>

namespace ebur128::test_signals {
namespace {

struct segment {
  double seconds;
  std::vector<double> dbfs;
};

struct signal_data {
  unsigned long samplerate;
  unsigned int channels;
  std::vector<double> samples;
};

segment tone(double seconds, std::initializer_list<double> dbfs) {
  return segment{seconds, dbfs};
}

segment silence(double seconds) { return segment{seconds, {}}; }

std::vector<segment> segments_for(int number, int variant) {
  if ((number == 10 || number == 13) && (variant < 1 || variant > 20)) {
    throw std::invalid_argument("invalid test signal variant");
  }
  if (number != 10 && number != 13 && variant != 0) {
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

unsigned int channels_for(int number) {
  return number == 6 ? 5 : test_signal::default_channels;
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

std::size_t frames_in_segment(const segment& value) {
  return static_cast<std::size_t>(std::llround(
      value.seconds * static_cast<double>(test_signal::default_samplerate)));
}

signal_data make_signal(const std::vector<segment>& segments,
                        unsigned int channels) {
  std::size_t frames = 0;
  for (const auto& value : segments) {
    frames += frames_in_segment(value);
  }

  signal_data result{
      test_signal::default_samplerate,
      channels,
      std::vector<double>(frames * channels),
  };

  std::size_t frame = 0;
  for (const auto& value : segments) {
    const auto segment_frames = frames_in_segment(value);
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
      const double phase = 2.0 * std::numbers::pi *
                           test_signal::default_frequency *
                           static_cast<double>(frame) /
                           static_cast<double>(test_signal::default_samplerate);

      for (unsigned int channel = 0; channel < channels; ++channel) {
        result.samples[frame * channels + channel] =
            amplitudes[channel] * std::sin(phase);
      }
    }
  }

  return result;
}

} // namespace

test_signal::test_signal(kind type, int number, int variant) {
  signal_data data;

  switch (type) {
    case kind::ebu3341:
      data = make_signal(segments_for(number, variant), channels_for(number));
      break;
    case kind::ebu3342:
      if (variant != 0)
        throw std::invalid_argument("invalid test signal variant");
      data = make_signal(lra_segments_for(number), default_channels);
      break;
    default: throw std::invalid_argument("unknown test signal kind");
  }

  samplerate_ = data.samplerate;
  channels_ = data.channels;
  samples_ = std::move(data.samples);
}

std::size_t test_signal::frames_for_seconds(double seconds) const {
  return static_cast<std::size_t>(
      std::llround(seconds * static_cast<double>(samplerate_)));
}

} // namespace ebur128::test_signals

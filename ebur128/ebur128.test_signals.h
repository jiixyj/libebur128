#ifndef EBUR128_TEST_SIGNALS_H_
#define EBUR128_TEST_SIGNALS_H_

#include <cstddef>
#include <span>
#include <vector>

namespace ebur128::test_signals {

class test_signal {
public:
  enum class kind { ebu3341, ebu3342 };

  static constexpr unsigned long default_samplerate = 48000;
  static constexpr unsigned int default_channels = 2;
  static constexpr double default_frequency = 1000.0;

  explicit test_signal(kind type, int number, int variant = 0);

  unsigned long samplerate() const { return samplerate_; }
  unsigned int channels() const { return channels_; }
  std::span<const double> samples() const {
    return std::span<const double>(samples_);
  }

  std::size_t frames() const { return samples_.size() / channels_; }
  std::size_t frames_for_seconds(double seconds) const;

private:
  unsigned long samplerate_;
  unsigned int channels_;
  std::vector<double> samples_;
};

} // namespace ebur128::test_signals

#endif

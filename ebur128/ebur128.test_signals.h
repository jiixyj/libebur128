#ifndef EBUR128_TEST_SIGNALS_H_
#define EBUR128_TEST_SIGNALS_H_

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <variant>
#include <vector>

namespace ebur128::test_signals {

struct file_not_found : std::runtime_error {
  using std::runtime_error::runtime_error;
};

class wav_file {
public:
  explicit wav_file(std::span<const std::uint8_t> data);
  explicit wav_file(const std::filesystem::path& path);

  unsigned long samplerate() const { return samplerate_; }
  unsigned int channels() const { return channels_; }
  unsigned int bits() const { return bits_; }
  const std::vector<std::int16_t>& samples_16() const {
    return std::get<0>(samples_);
  }
  const std::vector<std::int32_t>& samples_32() const {
    return std::get<1>(samples_);
  }
  std::size_t frames() const {
    return (this->bits() == 16 ? this->samples_16().size()
                               : this->samples_32().size()) /
           this->channels();
  }
  std::int32_t sample_at(std::size_t i) const {
    return this->bits() == 16
               ? (static_cast<std::int32_t>(this->samples_16()[i]) << 16)
               : this->samples_32()[i];
  }

private:
  unsigned long samplerate_;
  unsigned int channels_;
  unsigned int bits_;
  std::variant<std::vector<std::int16_t>, std::vector<std::int32_t>> samples_;
};

//

template <class T>
concept sample_type = std::same_as<T, short> || std::same_as<T, int> ||
                      std::same_as<T, float> || std::same_as<T, double>;

enum class test_signal_kind { ebu3341, ebu3342 };

template <sample_type T> class test_signal {
public:
  using kind = test_signal_kind;

  static constexpr unsigned long default_samplerate = 48000;
  static constexpr unsigned int default_channels = 2;
  static constexpr double default_frequency = 1000.0;

  explicit test_signal(kind type, int number, int variant = 0);

  unsigned long samplerate() const { return samplerate_; }
  unsigned int channels() const { return channels_; }
  std::span<const T> samples() const { return samples_; }

  std::size_t frames() const { return samples_.size() / channels_; }
  std::size_t frames_for_seconds(double seconds) const;

private:
  unsigned long samplerate_;
  unsigned int channels_;
  std::vector<T> samples_;
};

} // namespace ebur128::test_signals

#endif

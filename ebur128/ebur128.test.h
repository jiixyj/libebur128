#ifndef EBUR128_TEST_H_
#define EBUR128_TEST_H_

#include <ranges>
#include <stdexcept>
#include <utility>

#include <ebur128.h>
#include <ebur128.test_signals.h>

namespace ebur128::test {

// clang-format off
class error : public std::runtime_error { using std::runtime_error::runtime_error; };
class nomem_error                 : public error { using ebur128::test::error::error; };
class invalid_mode_error          : public error { using ebur128::test::error::error; };
class invalid_channel_index_error : public error { using ebur128::test::error::error; };
class no_change_error             : public error { using ebur128::test::error::error; };
// clang-format on

//

enum class channel : unsigned int {};
enum class samplerate : unsigned long {};
enum class mode : int {
  m = EBUR128_MODE_M,
  s = EBUR128_MODE_S,
  i = EBUR128_MODE_I,
  lra = EBUR128_MODE_LRA,
  sample_peak = EBUR128_MODE_SAMPLE_PEAK,
  true_peak = EBUR128_MODE_TRUE_PEAK,
  histogram = EBUR128_MODE_HISTOGRAM,
};

template <class T>
constexpr std::underlying_type_t<T> to_underlying(T val) noexcept {
  return static_cast<std::underlying_type_t<T>>(val);
}

inline ebur128::test::mode operator|(ebur128::test::mode lhs,
                                     ebur128::test::mode rhs) {
  return ebur128::test::mode{ebur128::test::to_underlying(lhs) |
                             ebur128::test::to_underlying(rhs)};
}

//

template <ebur128::test_signals::sample_type T> class audio_data_view {
public:
  explicit audio_data_view(std::span<const T> data, channel nr_channels)
      : data_(data), channels_(nr_channels) {
    if (data_.size() % ebur128::test::to_underlying(channels_))
      throw std::logic_error("audio_data_view");
  }

  std::span<const T> data() const { return data_; }
  std::size_t frames() const {
    return data_.size() / ebur128::test::to_underlying(channels_);
  }

private:
  std::span<const T> data_;
  ebur128::test::channel channels_;
};

template <std::ranges::contiguous_range R>
audio_data_view(R&&, ebur128::test::channel)
    -> audio_data_view<std::remove_const_t<std::ranges::range_value_t<R>>>;

//

class state {
public:
  explicit state() = default;
  explicit state(ebur128::test::channel channels,
                 ebur128::test::samplerate samplerate,
                 ebur128::test::mode mode)
      : state_(ebur128_init(ebur128::test::to_underlying(channels),
                            ebur128::test::to_underlying(samplerate),
                            ebur128::test::to_underlying(mode))) {
    if (!state_)
      throw ebur128::test::error("ebur128_init");
  }
  state(state&& other) noexcept
      : state_(std::exchange(other.state_, nullptr)) {}
  state& operator=(state&& other) noexcept {
    state(std::move(other)).swap(*this);
    return *this;
  }
  ~state() {
    if (state_)
      ebur128_destroy(&state_);
  }

  template <ebur128::test_signals::sample_type T>
  void add_frames(audio_data_view<T> data);

  void set_channel(ebur128::test::channel channel_index, ::channel channel) {
    state::handle_error(
        [&] {
          return ebur128_set_channel(
              state_, ebur128::test::to_underlying(channel_index), channel);
        },
        "ebur128_set_channel");
  }

  void set_max_window(std::chrono::milliseconds window) {
    state::handle_error(
        [&] {
          return ebur128_set_max_window(
              state_, static_cast<unsigned long>(window.count()));
        },
        "ebur128_set_max_window");
  }

  void set_max_history(std::chrono::milliseconds window) {
    state::handle_error(
        [&] {
          return ebur128_set_max_history(
              state_, static_cast<unsigned long>(window.count()));
        },
        "ebur128_set_max_history");
  }

  double loudness_window(std::chrono::milliseconds window) const {
    double out;
    state::handle_error(
        [&] {
          return ebur128_loudness_window(
              state_, static_cast<unsigned long>(window.count()), &out);
        },
        "ebur128_loudness_window");
    return out;
  }

  double loudness_global() const {
    double out;
    state::handle_error([&] { return ebur128_loudness_global(state_, &out); },
                        "ebur128_loudness_global");
    return out;
  }

  double relative_threshold() const {
    double out;
    state::handle_error(
        [&] { return ebur128_relative_threshold(state_, &out); },
        "ebur128_relative_threshold");
    return out;
  }

  template <std::ranges::range R>
    requires std::same_as<state, std::ranges::range_value_t<R>>
  static double loudness_global_multiple(R&& states) {
    auto states_ptrs_view =
        std::forward<R>(states) | std::views::transform([](const auto& state) {
          return state.state_ptr();
        });
    std::vector<ebur128_state*> states_ptrs(
        std::ranges::begin(states_ptrs_view),
        std::ranges::end(states_ptrs_view));

    double out;
    state::handle_error(
        [&] {
          return ebur128_loudness_global_multiple(states_ptrs.data(),
                                                  states_ptrs.size(), &out);
        },
        "ebur128_loudness_global_multiple");
    return out;
  }

  double loudness_range() const {
    double out;
    state::handle_error([&] { return ebur128_loudness_range(state_, &out); },
                        "ebur128_loudness_range");
    return out;
  }

  template <std::ranges::range R>
    requires std::same_as<state, std::ranges::range_value_t<R>>
  static double loudness_range_multiple(R&& states) {
    auto states_ptrs_view =
        std::forward<R>(states) | std::views::transform([](const auto& state) {
          return state.state_ptr();
        });
    std::vector<ebur128_state*> states_ptrs(
        std::ranges::begin(states_ptrs_view),
        std::ranges::end(states_ptrs_view));

    double out;
    state::handle_error(
        [&] {
          return ebur128_loudness_range_multiple(states_ptrs.data(),
                                                 states_ptrs.size(), &out);
        },
        "ebur128_loudness_range_multiple");
    return out;
  }

  double loudness_shortterm() const {
    double out;
    state::handle_error(
        [&] { return ebur128_loudness_shortterm(state_, &out); },
        "ebur128_loudness_shortterm");
    return out;
  }

  double loudness_momentary() const {
    double out;
    state::handle_error(
        [&] { return ebur128_loudness_momentary(state_, &out); },
        "ebur128_loudness_momentary");
    return out;
  }

  void change_parameters(ebur128::test::channel channels,
                         ebur128::test::samplerate samplerate) {
    state::handle_error(
        [&] {
          return ebur128_change_parameters(
              state_, ebur128::test::to_underlying(channels),
              ebur128::test::to_underlying(samplerate));
        },
        "ebur128_change_parameters");
  }

  double true_peak(ebur128::test::channel channel) const {
    double out;
    state::handle_error(
        [&] {
          return ebur128_true_peak(state_,
                                   ebur128::test::to_underlying(channel), &out);
        },
        "ebur128_true_peak");
    return out;
  }

  double prev_true_peak(ebur128::test::channel channel) const {
    double out;
    state::handle_error(
        [&] {
          return ebur128_prev_true_peak(
              state_, ebur128::test::to_underlying(channel), &out);
        },
        "ebur128_prev_true_peak");
    return out;
  }

  double sample_peak(ebur128::test::channel channel) const {
    double out;
    state::handle_error(
        [&] {
          return ebur128_sample_peak(
              state_, ebur128::test::to_underlying(channel), &out);
        },
        "ebur128_sample_peak");
    return out;
  }

  double prev_sample_peak(ebur128::test::channel channel) const {
    double out;
    state::handle_error(
        [&] {
          return ebur128_prev_sample_peak(
              state_, ebur128::test::to_underlying(channel), &out);
        },
        "ebur128_prev_sample_peak");
    return out;
  }

  static std::tuple<int, int, int> version() {
    std::tuple<int, int, int> version;
    state::handle_error(
        [&] {
          return ebur128_get_version(&std::get<0>(version),
                                     &std::get<1>(version),
                                     &std::get<2>(version));
        },
        "ebur128_get_version");
    return version;
  }

private:
  template <class F> static void handle_error(F f, const char* msg) {
    if constexpr (std::is_same_v<decltype(f()), void>) {
      return f();
    } else {
      switch (static_cast<enum ::error>(f())) {
        case EBUR128_SUCCESS: //
          return;
        case EBUR128_ERROR_NOMEM: //
          throw ebur128::test::nomem_error(msg);
        case EBUR128_ERROR_INVALID_MODE: //
          throw ebur128::test::invalid_mode_error(msg);
        case EBUR128_ERROR_INVALID_CHANNEL_INDEX:
          throw ebur128::test::invalid_channel_index_error(msg);
        case EBUR128_ERROR_NO_CHANGE: //
          throw ebur128::test::no_change_error(msg);
      }

      throw ebur128::test::error(msg);
    }
  }

  void swap(state& other) noexcept { std::ranges::swap(state_, other.state_); }

  ebur128_state* state_ptr() const { return state_; }

  ebur128_state* state_ = nullptr;
};

static_assert(std::is_nothrow_default_constructible_v<state>);
static_assert(std::is_nothrow_move_constructible_v<state>);
static_assert(std::is_nothrow_move_assignable_v<state>);

template <> inline void state::add_frames<short>(audio_data_view<short> data) {
  state::handle_error(
      [&] {
        return ebur128_add_frames_short(state_, data.data().data(),
                                        data.frames());
      },
      "ebur128_add_frames_short");
}

template <> inline void state::add_frames<int>(audio_data_view<int> data) {
  state::handle_error(
      [&] {
        return ebur128_add_frames_int(state_, data.data().data(),
                                      data.frames());
      },
      "ebur128_add_frames_int");
}

template <> inline void state::add_frames<float>(audio_data_view<float> data) {
  state::handle_error(
      [&] {
        return ebur128_add_frames_float(state_, data.data().data(),
                                        data.frames());
      },
      "ebur128_add_frames_float");
}

template <>
inline void state::add_frames<double>(audio_data_view<double> data) {
  state::handle_error(
      [&] {
        return ebur128_add_frames_double(state_, data.data().data(),
                                         data.frames());
      },
      "ebur128_add_frames_double");
}

} // namespace ebur128::test

#endif

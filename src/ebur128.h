/* See LICENSE file for copyright and license details. */
#ifndef EBUR128_H_
#define EBUR128_H_

#ifdef EBUR128_USE_SPEEX_RESAMPLER
#  define OUTSIDE_SPEEX
#  define RANDOM_PREFIX ebur128
#  include "speex_resampler.h"
#endif

/** \file ebur128.h
 *  \brief libebur128 - a library for loudness measurement according to
 *         the EBU R128 standard.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>

/* This can be replaced by any BSD-like queue implementation. */
#include "queue.h"

/** @cond SLIST
 *  Declare a linked list for our block energies.
 */
SLIST_HEAD(ebur128_double_queue, ebur128_dq_entry);
struct ebur128_dq_entry {
  double z;
  SLIST_ENTRY(ebur128_dq_entry) entries;
};
/** @endcond */

/** \enum channel
 *  Use these values when setting the channel map with ebur128_set_channel().
 */
enum channel {
  EBUR128_UNUSED = 0,     /**< unused channel (for example LFE channel) */
  EBUR128_LEFT,           /**< left channel */
  EBUR128_RIGHT,          /**< right channel */
  EBUR128_CENTER,         /**< center channel */
  EBUR128_LEFT_SURROUND,  /**< left surround channel */
  EBUR128_RIGHT_SURROUND  /**< right surround channel */
};

/** \enum mode
 *  Use these values in ebur128_init (xor'ed). Try to use the lowest possible
 *  modes that suit your needs, as performance will be better.
 */
enum mode {
  EBUR128_MODE_M           =  1, /**< can call ebur128_loudness_momentary */
  EBUR128_MODE_S           =  3, /**< can call ebur128_loudness_shortterm */
  EBUR128_MODE_I           =  5, /**< can call ebur128_gated_loudness_*   */
  EBUR128_MODE_LRA         = 11, /**< can call ebur128_loudness_range     */
  EBUR128_MODE_SAMPLE_PEAK = 17  /**< can call ebur128_sample_peak        */
#ifdef EBUR128_USE_SPEEX_RESAMPLER
 ,EBUR128_MODE_TRUE_PEAK   = 33  /**< can call ebur128_sample_peak        */
#endif
};

/** \brief Contains information about the state of a loudness measurement.
 *
 *  You should not need to modify this struct directly. The contents can change
 *  between library versions. The following fields will always be available for
 *  reading:
 *    - mode
 *    - channels
 *    - samplerate
 */
typedef struct {
  /** The current mode. */
  int mode;
  /** Filtered audio data (used as ring buffer). */
  double* audio_data;
  /** Size of audio_data array. */
  size_t audio_data_frames;
  /** Current index for audio_data. */
  size_t audio_data_index;
  /** How many frames are needed for a gating block. Will correspond to 400ms
   *  of audio at initialization, and 100ms after the first block (75% overlap
   *  as specified in the 2011 revision of BS1770). */
  size_t needed_frames;
  /** The number of channels. */
  size_t channels;
  /** The channel map. Has as many elements as there are channels. */
  int* channel_map;
  /** The sample rate. */
  size_t samplerate;
  /** How many samples fit in 100ms (rounded). */
  size_t samples_in_100ms;
  /** BS.1770 filter coefficients (nominator). */
  double b[5];
  /** BS.1770 filter coefficients (denominator). */
  double a[5];
  /** BS.1770 filter state. */
  double v[5][5];
  /** Linked list of block energies. */
  struct ebur128_double_queue block_list;
  /** Linked list of 3s-block energies, used to calculate LRA. */
  struct ebur128_double_queue short_term_block_list;
  /** Keeps track of when a new short term block is needed. */
  size_t short_term_frame_counter;
  /** Maximum sample peak, one per channel */
  double* sample_peak;
  /** Maximum true peak, one per channel */
  double* true_peak;
#ifdef EBUR128_USE_SPEEX_RESAMPLER
  SpeexResamplerState* resampler;
#endif
  size_t oversample_factor;
  float* resampler_buffer_input;
  size_t resampler_buffer_input_frames;
  float* resampler_buffer_output;
  size_t resampler_buffer_output_frames;
} ebur128_state;

/** \brief Initialize library state.
 *
 *  @param channels the number of channels.
 *  @param samplerate the sample rate.
 *  @param mode see the mode enum for possible values.
 *  @return an initialized library state.
 */
ebur128_state* ebur128_init(size_t channels, size_t samplerate, int mode);

/** \brief Destroy library state.
 *
 *  @param st pointer to a library state.
 */
void ebur128_destroy(ebur128_state** st);

/** \brief Set channel type.
 *
 *  The default is:
 *  - 0 -> EBUR128_LEFT
 *  - 1 -> EBUR128_RIGHT
 *  - 2 -> EBUR128_CENTER
 *  - 3 -> EBUR128_UNUSED
 *  - 4 -> EBUR128_LEFT_SURROUND
 *  - 5 -> EBUR128_RIGHT_SURROUND
 *
 *  @param st library state.
 *  @param channel_number zero based channel index.
 *  @param value channel type from the "channel" enum.
 *  @return
 *    - 0 on success.
 *    - 1 if invalid channel index.
 */
int ebur128_set_channel(ebur128_state* st, size_t channel_number, int value);

/** \brief Change library parameters.
 *
 *  Note that the channel map will be reset when setting a different number of
 *  channels. The current unfinished block will be lost.
 *
 *  @param st library state.
 *  @param channels new number of channels.
 *  @param samplerate new sample rate.
 *  @return
 *    - 0 on success.
 *    - 1 on memory allocation error. The state will be invalid and must be
 *        destroyed.
 *    - 2 if channels and sample rate were not changed.
 */
int ebur128_change_parameters(ebur128_state* st,
                              size_t channels,
                              size_t samplerate);

/** \brief Add frames to be processed.
 *
 *  @param st library state.
 *  @param src array of source frames. Channels must be interleaved.
 *  @param frames number of frames. Not number of samples!
 *  @return
 *    - 0 on success.
 *    - 1 on memory allocation error.
 */
int ebur128_add_frames_short(ebur128_state* st,
                             const short* src,
                             size_t frames);
/** \brief See \ref ebur128_add_frames_short */
int ebur128_add_frames_int(ebur128_state* st,
                             const int* src,
                             size_t frames);
/** \brief See \ref ebur128_add_frames_short */
int ebur128_add_frames_float(ebur128_state* st,
                             const float* src,
                             size_t frames);
/** \brief See \ref ebur128_add_frames_short */
int ebur128_add_frames_double(ebur128_state* st,
                             const double* src,
                             size_t frames);

/** \brief Get global integrated loudness in LUFS.
 *
 *  @param st library state.
 *  @return integrated loudness in LUFS. NaN if mode "EBUR128_MODE_I" has not
 *          been set.
 */
double ebur128_loudness_global(ebur128_state* st);
/** \brief Get global integrated loudness in LUFS across multiple instances.
 *
 *  @param sts array of library states.
 *  @param size length of sts
 *  @return integrated loudness in LUFS. NaN if mode "EBUR128_MODE_I" has not
 *          been set.
 */
double ebur128_loudness_global_multiple(ebur128_state** sts, size_t size);

/** \brief Get momentary loudness (last 400ms) in LUFS.
 *
 *  @param st library state.
 *  @return momentary loudness in LUFS.
 */
double ebur128_loudness_momentary(ebur128_state* st);
/** \brief Get short-term loudness (last 3s) in LUFS.
 *
 *  @param st library state.
 *  @return short-term loudness in LUFS. NaN if mode "EBUR128_MODE_S" has not
 *          been set.
 */
double ebur128_loudness_shortterm(ebur128_state* st);

/** \brief Get loudness range (LRA) of programme in LU.
 *
 *  Calculates loudness range according to EBU 3342.
 *
 *  @param st library state.
 *  @return loudness range (LRA) in LU. NaN if memory allocation fails or mode
 *          "EBUR128_MODE_LRA" has not been set.
 */
double ebur128_loudness_range(ebur128_state* st);
/** \brief Get loudness range (LRA) in LU across multiple instances.
 *
 *  Calculates loudness range according to EBU 3342.
 *
 *  @param sts array of library states.
 *  @param size length of sts
 *  @return loudness range (LRA) in LU. NaN if memory allocation fails or mode
 *          "EBUR128_MODE_LRA" has not been set.
 */
double ebur128_loudness_range_multiple(ebur128_state** sts, size_t size);

double ebur128_sample_peak(ebur128_state*, size_t channel_number);
#ifdef EBUR128_USE_SPEEX_RESAMPLER
double ebur128_true_peak(ebur128_state*, size_t channel_number);
double ebur128_dbtp(ebur128_state*, size_t channel_number);
#endif

#ifdef __cplusplus
}
#endif

#endif  /* EBUR128_H_ */

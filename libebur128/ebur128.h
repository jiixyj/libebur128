/* See LICENSE file for copyright and license details. */
#ifndef EBUR128_H_
#define EBUR128_H_

#ifndef EBUR128_USE_SPEEX_RESAMPLER
  #define EBUR128_USE_SPEEX_RESAMPLER 1
#endif

/** \file ebur128.h
 *  \brief libebur128 - a library for loudness measurement according to
 *         the EBU R128 standard.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>       /* for size_t */

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
#if EBUR128_USE_SPEEX_RESAMPLER
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
struct ebur128_state_internal;
typedef struct {
  /** The current mode. */
  int mode;
  /** The number of channels. */
  size_t channels;
  /** The sample rate. */
  size_t samplerate;
  struct ebur128_state_internal* d;
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

/** \brief Get maximum sample peak of selected channel in float format.
 *
 *  @param st library state
 *  @param channel_number channel to analyse
 *  @return maximum sample peak in float format (1.0 is 0 dBFS)
 */
double ebur128_sample_peak(ebur128_state* st, size_t channel_number);
#if EBUR128_USE_SPEEX_RESAMPLER
/** \brief Get maximum true peak of selected channel in float format.
 *
 *  Uses the Speex resampler with quality level 8 to calculate true peak. Will
 *  oversample 4x for sample rates < 96000 Hz, 2x for sample rates < 192000 Hz
 *  and leave the signal unchanged for 192000 Hz.
 *
 *  @param st library state
 *  @param channel_number channel to analyse
 *  @return maximum true peak in float format (1.0 is 0 dBFS)
 */
double ebur128_true_peak(ebur128_state* st, size_t channel_number);
#endif

#ifdef __cplusplus
}
#endif

#endif  /* EBUR128_H_ */

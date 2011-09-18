/* See LICENSE file for copyright and license details. */
#ifndef _RGTAG_H_
#define _RGTAG_H_

#ifdef __cplusplus
extern "C" {
#endif

int set_rg_info(const char* filename,
                double track_gain,
                double track_peak,
                int album_mode,
                double album_gain,
                double album_peak);

#ifdef __cplusplus
}
#endif

#endif  /* _RGTAG_H_ */

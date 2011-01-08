#include <math.h>
#include <sndfile.h>
#include <string.h>
#include <stdlib.h>

#include "./ebur128.h"

#define CHECK_ERROR(condition, message, errorcode, goto_point)                 \
  if ((condition)) {                                                           \
    fprintf(stderr, message);                                                  \
    errcode = (errorcode);                                                     \
    goto goto_point;                                                           \
  }


int main(int ac, const char* av[]) {
  SF_INFO file_info;
  SNDFILE* file;
  SNDFILE* file_out;
  double* audio_data;
  int audio_data_half = 0;
  sf_count_t nr_frames;
  sf_count_t nr_frames_read;
  sf_count_t nr_frames_read_all = 0;
  sf_count_t nr_frames_written;
  double** v;
  double** v2;
  double* z;
  double** zg;
  size_t zg_index = 0;
  double loudness = 0.0;
  double* lg;
  int i;
  int errcode = 0;
  int result;

  CHECK_ERROR(ac != 2, "usage: r128-test FILENAME\n", 1, exit)

  memset(&file_info, '\0', sizeof(file_info));
  file = sf_open(av[1], SFM_READ, &file_info);
  CHECK_ERROR(!file, "Could not open input file!\n", 1, exit)
  nr_frames = file_info.frames;

  file_out = sf_open("out.wav", SFM_WRITE, &file_info);
  CHECK_ERROR(!file_out, "Could not open output file!\n", 1, close_file)
  file_info.frames = nr_frames;

  audio_data = (double*) malloc((size_t) 19200 * 2
                              * (size_t) file_info.channels
                              * sizeof(double));
  CHECK_ERROR(!audio_data, "Could not allocate memory!\n", 1, close_file_out)

  result = init_filter_state(&v, file_info.channels, 3);
  CHECK_ERROR(result, "Could not initialize filter!\n", 1, free_audio_data)
  result = init_filter_state(&v2, file_info.channels, 3);
  CHECK_ERROR(result, "Could not initialize filter!\n", 1, release_filter_state_1)

  z = (double*) calloc((size_t) file_info.channels, sizeof(double));
  CHECK_ERROR(!z, "Could not initialize z!\n", 1, release_filter_state_2)

  result = init_filter_state(&zg, file_info.channels, file_info.frames / 9600 - 1);
  CHECK_ERROR(result, "Could not initialize z!\n", 1, free_z)

  lg = (double*) calloc((size_t) file_info.frames / 9600 - 1, sizeof(double));
  CHECK_ERROR(!lg, "Could not initialize lg!\n", 1, free_zg)

  while ((nr_frames_read = sf_readf_double(file, audio_data +
                                                 audio_data_half * 19200 *
                                                 file_info.channels,
                                           19200))) {
    nr_frames_read_all += nr_frames_read;
    result = do_stuff(audio_data + audio_data_half * 19200 *
                                   file_info.channels,
                      (size_t) nr_frames_read, file_info.channels,
                      v, v2, z);
    CHECK_ERROR(result, "Calculation failed!\n", 1, free_lg)

    if (audio_data_half == 0) {
      if (zg_index != 0) {
        if (nr_frames_read < 9600) break;
        memcpy(audio_data + 19200 * file_info.channels,
               audio_data,
               9600 * file_info.channels * sizeof(double));
        calc_gating_block(audio_data + 19200 * file_info.channels,
                          nr_frames_read, file_info.channels,
                          zg, zg_index);
        ++zg_index;
      }
      if (nr_frames_read < 19200) break;
      calc_gating_block(audio_data, nr_frames_read, file_info.channels,
                        zg, zg_index);
      ++zg_index;
    } else {
      if (nr_frames_read < 9600) break;
      calc_gating_block(audio_data + 9600 * file_info.channels,
                        nr_frames_read, file_info.channels,
                        zg, zg_index);
      ++zg_index;
      if (nr_frames_read < 19200) break;
      calc_gating_block(audio_data + 19200 * file_info.channels,
                        nr_frames_read, file_info.channels,
                        zg, zg_index);
      ++zg_index;
    }

    nr_frames_written = sf_writef_double(file_out,
                                         audio_data + audio_data_half * 19200 *
                                                      file_info.channels,
                                         nr_frames_read);
    CHECK_ERROR(nr_frames_written != nr_frames_read,
                "Could not write to file!\n"
                "File system full?\n", 1, free_lg)
    audio_data_half = audio_data_half ? 0 : 1;
  }
  CHECK_ERROR(file_info.frames != nr_frames_read_all,
              "Could not read full file!\n", 1, free_lg)

  double relative_threshold, gated_loudness;
  calculate_block_loudness(lg, zg, file_info.frames, file_info.channels);
  calculate_relative_threshold(&relative_threshold, lg, zg, file_info.frames, file_info.channels);
  calculate_gated_loudness(&gated_loudness, relative_threshold, lg, zg, file_info.frames, file_info.channels);

  fprintf(stderr, "relative threshold: %f LKFS\n", relative_threshold);
  fprintf(stderr, "gated loudness: %f LKFS\n", gated_loudness);


free_lg:
  free(lg);

free_zg:
  release_filter_state(&zg, file_info.channels);

free_z:
  free(z);

release_filter_state_2:
  release_filter_state(&v2, file_info.channels);

release_filter_state_1:
  release_filter_state(&v, file_info.channels);

free_audio_data:
  free(audio_data);

close_file_out:
  if (sf_close(file_out)) {
    fprintf(stderr, "Could not close output file!\n");
  }

close_file:
  if (sf_close(file)) {
    fprintf(stderr, "Could not close input file!\n");
  }

exit:
  return errcode;
}

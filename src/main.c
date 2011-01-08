#include <math.h>
#include <sndfile.h>
#include <string.h>
#include <stdlib.h>

#define CHECK_ERROR(condition, message, errorcode, goto_point)                 \
  if ((condition)) {                                                           \
    fprintf(stderr, message);                                                  \
    errcode = (errorcode);                                                     \
    goto goto_point;                                                           \
  }

int filter(double* dest, const double* source,
           size_t frames, int channels, int c,
           const double* b,
           const double* a,
           double** v) {
  size_t i;
  for (i = 0; i < frames; ++i) {
    v[c][0] = source[i * (size_t) channels + (size_t) c]
                - a[1] * v[c][1]
                - a[2] * v[c][2];
    dest[i * (size_t) channels + (size_t) c] =
                  b[0] * v[c][0]
                + b[1] * v[c][1]
                + b[2] * v[c][2];
    memmove(&v[c][1], &v[c][0], 2 * sizeof(double));
  }
  return 0;
}

int do_stuff(double* audio_data, size_t frames, int channels,
             double** v, double** v2,
             double* z) {

  static double b[] = {1.53512485958697, -2.69169618940638, 1.19839281085285};
  static double a[] = {1.0, -1.69065929318241, 0.73248077421585};
  static double b2[] = {1.0, -2.0, 1.0};
  static double a2[] = {1.0, -1.99004745483398, 0.99007225036621};
  int c;
  size_t i;
  double tmp;
  for (c = 0; c < channels; ++c) {
    filter(audio_data, audio_data,
           frames, channels, c,
           b, a,
           v);
    filter(audio_data, audio_data,
           frames, channels, c,
           b2, a2,
           v2);
    tmp = 0.0;
    for (i = 0; i < frames; ++i) {
      tmp += audio_data[i * (size_t) channels + (size_t) c] *
             audio_data[i * (size_t) channels + (size_t) c];
    }
    z[c] += tmp;
  }

  return 0;
}

int init_filter_state(double*** v, int channels, int filter_size) {
  int i, errcode = 0;
  *v = (double**) calloc((size_t) channels, sizeof(double*));
  CHECK_ERROR(!(*v), "Could not allocate memory!\n", 1, exit)
  for (i = 0; i < channels; ++i) {
    (*v)[i] = (double*) calloc((size_t) filter_size, sizeof(double));
    CHECK_ERROR(!((*v)[i]), "Could not allocate memory!\n", 1, free_all)
  }
  return 0;

free_all:
  for (i = 0; i < channels; ++i) {
    free((*v)[i]);
  }
  free(*v);
exit:
  return errcode;
}

void release_filter_state(double*** v, int channels) {
  int i;
  for (i = 0; i < channels; ++i) {
    free((*v)[i]);
  }
  free(*v);
}

int main(int ac, const char* av[]) {
  SF_INFO file_info;
  SNDFILE* file;
  SNDFILE* file_out;
  double* audio_data;
  sf_count_t nr_frames;
  sf_count_t nr_frames_read;
  sf_count_t nr_frames_read_all = 0;
  sf_count_t nr_frames_written;
  double** v;
  double** v2;
  double* z;
  double loudness = 0.0;
  int i;
  int errcode = 0;
  int result;

  CHECK_ERROR(ac != 2, "usage: r128-test FILENAME\n", 1, exit)

  file = sf_open(av[1], SFM_READ, &file_info);
  CHECK_ERROR(!file, "Could not open input file!\n", 1, exit)
  nr_frames = file_info.frames;

  file_out = sf_open("out.wav", SFM_WRITE, &file_info);
  CHECK_ERROR(!file_out, "Could not open output file!\n", 1, close_file)
  file_info.frames = nr_frames;

  audio_data = (double*) malloc((size_t) file_info.samplerate * 10
                              * (size_t) file_info.channels
                              * sizeof(double));
  CHECK_ERROR(!audio_data, "Could not allocate memory!\n", 1, close_file_out)

  result = init_filter_state(&v, file_info.channels, 3);
  CHECK_ERROR(result, "Could not initialize filter!\n", 1, free_audio_data)
  result = init_filter_state(&v2, file_info.channels, 3);
  CHECK_ERROR(result, "Could not initialize filter!\n", 1, release_filter_state_1)

  z = (double*) calloc((size_t) file_info.channels, sizeof(double));
  CHECK_ERROR(!z, "Could not allocate memory!\n", 1, release_filter_state_2)

  while ((nr_frames_read = sf_readf_double(file, audio_data,
                                           file_info.samplerate * 10))) {
    nr_frames_read_all += nr_frames_read;
    result = do_stuff(audio_data, (size_t) nr_frames_read, file_info.channels,
                      v, v2, z);
    CHECK_ERROR(result, "Calculation failed!\n", 1, free_z)

    nr_frames_written = sf_writef_double(file_out, audio_data, nr_frames_read);
    CHECK_ERROR(nr_frames_written != nr_frames_read,
                "Could not write to file!\n"
                "File system full?\n", 1, free_z)
  }
  CHECK_ERROR(file_info.frames != nr_frames_read_all,
              "Could not read full file!\n", 1, free_z)

  for (i = 0; i < file_info.channels; ++i) {
    z[i] /= (double) nr_frames_read_all;
    fprintf(stderr, "channel %d: %f\n", i, z[i]);
  }

  for (i = 0; i < file_info.channels; ++i) {
    switch (i) {
      case 0: case 1: case 2:
        break;
      case 4: case 5:
        z[i] *= 1.41;
        break;
      default:
        z[i] *= 0;
    }
    loudness += z[i];
  }
  loudness = 10 * (log(loudness) / log(10.0));
  loudness -= 0.691;
  fprintf(stderr, "loudness: %f LKFS\n", loudness);


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

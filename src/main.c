#include <sndfile.h>
#include <stdlib.h>

#define CHECK_ERROR(condition, message, errorcode, goto_point)                 \
  if ((condition)) {                                                           \
    fprintf(stderr, message);                                                  \
    errcode = (errorcode);                                                     \
    goto goto_point;                                                           \
  }

int filter(double* dest, const double* source,
           size_t frames, int channels, int channel,
           const double* b,
           const double* a,
           double* v,
           size_t* v_index,
           size_t filter_size) {
  size_t i;
  for (i = 0; i < frames; ++i) {
    v[*v_index] = source[i * channels + channel]
                - a[1] * v[(*v_index + filter_size - 1) % filter_size]
                - a[2] * v[(*v_index + filter_size - 2) % filter_size];
    dest[i * channels + channel] =
                  b[0] * v[*v_index]
                + b[1] * v[(*v_index + filter_size - 1) % filter_size]
                + b[2] * v[(*v_index + filter_size - 2) % filter_size];
    *v_index = (*v_index + 1) % filter_size;
  }
  return 0;
}

int do_stuff(double* audio_data, SF_INFO* file_info) {

  static double b[] = {1.53512485958697, -2.69169618940638, 1.19839281085285};
  static double a[] = {1.0, -1.69065929318241, 0.73248077421585};
  static double b2[] = {1.0, -2.0, 1.0};
  static double a2[] = {1.0, -1.99004745483398, 0.99007225036621};
  int c;
  for (c = 0; c < file_info->channels; ++c) {
    double v[] = {0.0, 0.0, 0.0};
    size_t v_index = 0;
    double v2[] = {0.0, 0.0, 0.0};
    size_t v2_index = 0;
    filter(audio_data, audio_data,
           file_info->frames, file_info->channels, c,
           b, a,
           v, &v_index,
           3);
    filter(audio_data, audio_data,
           file_info->frames, file_info->channels, c,
           b2, a2,
           v2, &v2_index,
           3);
  }

  return 0;
}

int main(int ac, const char* av[]) {
  SF_INFO file_info;
  SNDFILE* file;
  SNDFILE* file_out;
  double* audio_data;
  sf_count_t nr_frames_read;
  sf_count_t nr_frames_written;
  int errcode = 0;
  int result;

  CHECK_ERROR(ac != 2, "usage: r128-test FILENAME\n", 1, exit)

  file = sf_open(av[1], SFM_READ, &file_info);
  CHECK_ERROR(!file, "Could not open input file!\n", 1, exit)

  audio_data = (double*) malloc((unsigned long) file_info.frames
                              * (unsigned long) file_info.channels
                              * sizeof(double));
  CHECK_ERROR(!audio_data, "Could not allocate memory!\n", 1, close_file)

  nr_frames_read = sf_readf_double(file, audio_data, file_info.frames);
  CHECK_ERROR(nr_frames_read != file_info.frames,
              "Could not read full file!\n", 1, free_audio_data)

  result = do_stuff(audio_data, &file_info);
  CHECK_ERROR(result, "Calculation failed!\n", 1, free_audio_data)

  file_out = sf_open("out.wav", SFM_WRITE, &file_info);
  CHECK_ERROR(!file_out, "Could not open output file!\n", 1, free_audio_data)

  nr_frames_written = sf_writef_double(file_out, audio_data, nr_frames_read);
  CHECK_ERROR(nr_frames_written != nr_frames_read,
              "Could not write full file!\n", 1, close_file_out)


close_file_out:
  if (sf_close(file_out)) {
    fprintf(stderr, "Could not close output file!\n");
  }

free_audio_data:
  free(audio_data);

close_file:
  if (sf_close(file)) {
    fprintf(stderr, "Could not close input file!\n");
  }

exit:
  return errcode;
}

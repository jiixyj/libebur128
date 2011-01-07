#include <sndfile.h>
#include <stdlib.h>

#define CHECK_ERROR(condition, message, errorcode, goto_point)                 \
  if ((condition)) {                                                           \
    fprintf(stderr, message);                                                  \
    errcode = (errorcode);                                                     \
    goto goto_point;                                                           \
  }

int do_stuff(int* audio_data, SF_INFO* file_info) {
  int i;
  for (i = 0; i < file_info->frames; ++i) {
    int c;
    for (c = 0; c < file_info->channels; ++c) {
      audio_data[i * file_info->channels + c] /= 2;
    }
  }
  return 0;
}

int main(int ac, const char* av[]) {
  SF_INFO file_info;
  SNDFILE* file;
  SNDFILE* file_out;
  int* audio_data;
  sf_count_t nr_frames_read;
  sf_count_t nr_frames_written;
  int errcode = 0;
  int result;

  CHECK_ERROR(ac != 2, "usage: r128-test FILENAME\n", 1, exit)

  file = sf_open(av[1], SFM_READ, &file_info);
  CHECK_ERROR(!file, "Could not open input file!\n", 1, exit)

  audio_data = (int*) malloc((unsigned long) file_info.frames
                           * (unsigned long) file_info.channels
                           * sizeof(int));
  CHECK_ERROR(!audio_data, "Could not allocate memory!\n", 1, close_file)

  nr_frames_read = sf_readf_int(file, audio_data, file_info.frames);
  CHECK_ERROR(nr_frames_read != file_info.frames,
              "Could not read full file!\n", 1, free_audio_data)

  result = do_stuff(audio_data, &file_info);
  CHECK_ERROR(result, "Calculation failed!\n", 1, free_audio_data)

  file_out = sf_open("out.wav", SFM_WRITE, &file_info);
  CHECK_ERROR(!file_out, "Could not open output file!\n", 1, free_audio_data)

  nr_frames_written = sf_writef_int(file_out, audio_data, nr_frames_read);
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

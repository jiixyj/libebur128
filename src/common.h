#ifndef _COMMON_H_
#define _COMMON_H_

#define CHECK_ERROR(condition, message, errorcode, goto_point)                 \
  if ((condition)) {                                                           \
    fprintf(stderr, message);                                                  \
    errcode = (errorcode);                                                     \
    goto goto_point;                                                           \
  }

struct gain_data {
  char* const* file_names;
  int calculate_lra, tag_rg;
  ebur128_state** library_states;
  double* segment_loudness;
  double* segment_peaks;
};

#endif  /* _COMMON_H_ */

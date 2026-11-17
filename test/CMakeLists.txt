if(ENABLE_TESTS)
  find_package(PkgConfig REQUIRED)
  find_library(MATH_LIBRARY m)

  pkg_check_modules(SNDFILE REQUIRED IMPORTED_TARGET sndfile)
  include_directories(SYSTEM ${SNDFILE_INCLUDE_DIRS})

  add_executable(r128-test-library tests.c)
  target_link_libraries(r128-test-library PRIVATE ebur128 PkgConfig::SNDFILE)
  if(MATH_LIBRARY)
    target_link_libraries(r128-test-library ${MATH_LIBRARY})
  endif()
endif()

if(ENABLE_FUZZER)
  include_directories(${EBUR128_INCLUDE_DIR})

  add_executable(fuzzer fuzzer.c)
  target_link_libraries(fuzzer ebur128)
  target_compile_options(fuzzer PUBLIC "${FUZZER_FLAGS}")
  target_link_libraries(fuzzer "${FUZZER_FLAGS}")
endif()

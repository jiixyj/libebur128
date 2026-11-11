set(ENABLE_TESTS OFF CACHE BOOL "Build test binaries, needs libsndfile")
set(ENABLE_FUZZER OFF CACHE BOOL "Build fuzzer binary (requires clang++)")

if(ENABLE_TESTS)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SNDFILE REQUIRED IMPORTED_TARGET sndfile)

  add_executable(r128-test-library tests.c)
  add_executable(minimal-example minimal-example.c)

  target_link_libraries(r128-test-library PRIVATE ebur128 PkgConfig::SNDFILE)
  target_link_libraries(minimal-example PRIVATE ebur128 PkgConfig::SNDFILE)
endif()

if(ENABLE_FUZZER)
  include_directories(${EBUR128_INCLUDE_DIR})

  add_executable(fuzzer fuzzer.c)
  target_link_libraries(fuzzer ebur128)
  target_compile_options(fuzzer PUBLIC "${FUZZER_FLAGS}")
  target_link_libraries(fuzzer "${FUZZER_FLAGS}")
endif()

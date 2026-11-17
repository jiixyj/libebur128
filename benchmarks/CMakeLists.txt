add_executable(ebur128-benchmark ebur128-benchmark.cpp)
target_link_libraries(ebur128-benchmark PRIVATE
  ebur128::ebur128 ebur128::ebur128.test)

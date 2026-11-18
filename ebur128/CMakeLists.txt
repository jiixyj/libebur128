set(WITH_STATIC_PIC         OFF CACHE BOOL "Compile static library with -fPIC flag")

#### queue.h
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/queuetest.c
"#include <sys/queue.h>\nLIST_HEAD(listhead, entry) head;\nint main() { return 0; }")
try_compile(HAS_QUEUE ${CMAKE_CURRENT_BINARY_DIR}
                      ${CMAKE_CURRENT_BINARY_DIR}/queuetest.c)

set(SUMMARY_HAS_QUEUE ${HAS_QUEUE} CACHE INTERNAL "")

include(CMakeDependentOption)
cmake_dependent_option(ENABLE_INTERNAL_QUEUE_H "Use own queue.h" OFF "HAS_QUEUE" ON)
# PARENT_SCOPE set necessary due to status print in parent CMakeLists.txt
set(ENABLE_INTERNAL_QUEUE_H ${ENABLE_INTERNAL_QUEUE_H} PARENT_SCOPE)

if(ENABLE_INTERNAL_QUEUE_H)
  include_directories(SYSTEM queue)
endif()


if(MSVC)
  add_definitions(-D_USE_MATH_DEFINES)
  if(CMAKE_SIZEOF_VOID_P LESS 8)
    add_definitions(/arch:SSE2)
  endif()
endif()


set(EBUR128_VERSION_MAJOR 1)
set(EBUR128_VERSION 1.2.6)

add_library(ebur128)
add_library(ebur128::ebur128 ALIAS ebur128)
set_target_properties(ebur128 PROPERTIES
  SOVERSION ${EBUR128_VERSION_MAJOR}
  VERSION ${EBUR128_VERSION}
)
if(BUILD_SHARED_LIBS)
  if(MSVC)
    target_sources(ebur128 PRIVATE ebur128.def)
  endif()
else()
  if(WITH_STATIC_PIC)
    set_property(TARGET ebur128 PROPERTY POSITION_INDEPENDENT_CODE ON)
  endif()
endif()

# Link with Math library if needed.
include(CheckSymbolExists)
include(CMakePushCheckState)
check_symbol_exists(pow "math.h" HAVE_LIBC_MATH)
if(NOT HAVE_LIBC_MATH)
  cmake_push_check_state()
  list(APPEND CMAKE_REQUIRED_LIBRARIES m)
  check_symbol_exists(pow "math.h" HAVE_LIBM_MATH)
  cmake_pop_check_state()
endif()

set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads)

function(ebur128_configure_library target)
  set(source_args)
  if("CXX" IN_LIST ARGN)
    target_sources(${target} PRIVATE FILE_SET SOURCES)
    set_property(FILE_SET SOURCES TARGET ${target} PROPERTY LANGUAGE CXX)
    set(source_args FILE_SET SOURCES FILES)
  endif()

  target_sources(${target}
    PRIVATE ${source_args} ebur128.c
    PUBLIC FILE_SET HEADERS FILES ebur128.h
    PRIVATE FILE_SET private_headers TYPE HEADERS FILES ebur128/once.h
  )

  if(WIN32)
    target_sources(${target}
      PRIVATE ${source_args} ebur128/once.windows.c)
  elseif(TARGET Threads::Threads)
    target_sources(${target}
      PRIVATE ${source_args} ebur128/once.pthread.c)
    target_link_libraries(${target} PRIVATE Threads::Threads)
  else()
    target_sources(${target}
      PRIVATE ${source_args} ebur128/once.simple.c)
  endif()

  target_link_libraries(${target} PRIVATE $<$<BOOL:${HAVE_LIBM_MATH}>:m>)
endfunction()

ebur128_configure_library(ebur128)

# Check that the code is in the C/C++ language subset by adding this otherwise
# unused target that uses a C++ compiler to build all `.c` files.
if(BUILD_TESTING AND CMAKE_VERSION VERSION_GREATER_EQUAL 4.5)
  add_library(ebur128.cpp STATIC)
  ebur128_configure_library(ebur128.cpp CXX)
  target_compile_features(ebur128.cpp PRIVATE cxx_std_20)
endif()

if(BUILD_TESTING OR EBUR128_BUILD_BENCHMARKS)
  add_library(ebur128.test INTERFACE)
  add_library(ebur128::ebur128.test ALIAS ebur128.test)
  target_sources(ebur128.test INTERFACE
    FILE_SET HEADERS FILES ebur128.test.h)
  target_compile_features(ebur128.test INTERFACE cxx_std_20)
endif()

macro(ebur128_add_test test_target module)
  if(CMAKE_VERSION VERSION_GREATER_EQUAL 4.4)
    discover_tests(COMMAND ${test_target}
      DISCOVERY_ARGS "--list-tests"
      DISCOVERY_MATCH "^(.*)$"
      TEST_NAME "${module}-\\1"
      TEST_ARGS --run-test "\\1"
      TEST_PROPERTIES
        WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
        SKIP_RETURN_CODE 77
    )
  else()
    add_test(NAME ${test_target} COMMAND ${test_target})
    set_tests_properties(${test_target} PROPERTIES
      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
      SKIP_RETURN_CODE 77
    )
  endif()
endmacro()

if(BUILD_TESTING)
  option(EBUR128_TEST_PERFORMANCE
         "Enable timing-based FTZ performance test" ON)
  option(EBUR128_TEST_MALLOC_FAIL
         "Enable malloc/calloc failure injection test" ON)

  add_library(ebur128.test_signals STATIC)
  add_library(ebur128::ebur128.test_signals ALIAS ebur128.test_signals)
  target_sources(
      ebur128.test_signals
      PRIVATE ebur128.test_signals.cpp
      PUBLIC FILE_SET HEADERS FILES ebur128.test_signals.h
  )
  target_compile_features(ebur128.test_signals PRIVATE cxx_std_20)

  add_executable(ebur128-test ebur128-test.cpp)
  target_link_libraries(ebur128-test PRIVATE
    ebur128::ebur128 ebur128::ebur128.test ebur128::ebur128.test_signals)
  if(TARGET Threads::Threads)
    target_link_libraries(ebur128-test PRIVATE Threads::Threads)
  endif()
  ebur128_add_test(ebur128-test ebur128)

  if(EBUR128_TEST_PERFORMANCE)
    add_executable(ebur128-test.ftz_performance
      ebur128-test.ftz_performance.cpp)
    target_link_libraries(ebur128-test.ftz_performance PRIVATE
      ebur128::ebur128 ebur128::ebur128.test)
    ebur128_add_test(ebur128-test.ftz_performance ebur128)
  endif()

  add_executable(ebur128-test.true_peak ebur128-test.true_peak.cpp)
  target_link_libraries(ebur128-test.true_peak PRIVATE
    ebur128::ebur128 ebur128::ebur128.test ebur128::ebur128.test_signals)
  if(TARGET Threads::Threads)
    target_link_libraries(ebur128-test.true_peak PRIVATE Threads::Threads)
  endif()
  ebur128_add_test(ebur128-test.true_peak ebur128)

  add_executable(ebur128-test.ebu_files ebur128-test.ebu_files.cpp)
  target_link_libraries(ebur128-test.ebu_files PRIVATE
    ebur128::ebur128 ebur128::ebur128.test ebur128::ebur128.test_signals)
  if(TARGET Threads::Threads)
    target_link_libraries(ebur128-test.ebu_files PRIVATE Threads::Threads)
  endif()
  ebur128_add_test(ebur128-test.ebu_files ebur128)

  add_executable(ebur128.test_signals-test ebur128.test_signals-test.cpp)
  target_compile_features(ebur128.test_signals-test PRIVATE cxx_std_20)
  target_link_libraries(ebur128.test_signals-test PRIVATE ebur128.test_signals)
  add_test(NAME ebur128.test_signals-test
    COMMAND ebur128.test_signals-test
      "${PROJECT_BINARY_DIR}/ebu-loudness-test-setv05")
  set_tests_properties(ebur128.test_signals-test PROPERTIES SKIP_RETURN_CODE 77)

  if(EBUR128_TEST_MALLOC_FAIL)
    add_library(ebur128.malloc_fail STATIC)
    add_library(ebur128::ebur128.malloc_fail ALIAS ebur128.malloc_fail)
    ebur128_configure_library(ebur128.malloc_fail)
    set_target_properties(ebur128.malloc_fail PROPERTIES
      POSITION_INDEPENDENT_CODE ON)
    target_compile_definitions(ebur128.malloc_fail PRIVATE
      malloc=ebur128_test_malloc
      calloc=ebur128_test_calloc)

    add_executable(ebur128-test.malloc_fail ebur128-test.malloc_fail.cpp)
    target_link_libraries(ebur128-test.malloc_fail PRIVATE
      ebur128::ebur128.malloc_fail ebur128::ebur128.test)
    if(TARGET Threads::Threads)
      target_link_libraries(ebur128-test.malloc_fail PRIVATE Threads::Threads)
    endif()
    ebur128_add_test(ebur128-test.malloc_fail ebur128)
  endif()
endif()

install(TARGETS ebur128 FILE_SET HEADERS)

#### pkg-config
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/libebur128.pc.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/libebur128.pc @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/libebur128.pc"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

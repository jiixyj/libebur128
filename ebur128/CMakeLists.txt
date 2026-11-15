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
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm|ARM)")
  message(STATUS "Enabling ARM NEON support")
  if(CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang)$")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfpu=neon")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfpu=neon")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv7-a")
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv7-a")
    endif()
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

# Link with Math library if available
find_library(MATH_LIBRARY m)

set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads)

function(ebur128_configure_library target)
  target_sources(${target}
    PRIVATE ebur128.c
    PUBLIC FILE_SET HEADERS FILES ebur128.h
    PRIVATE FILE_SET private_headers TYPE HEADERS FILES ebur128/once.h
  )

  if(WIN32)
    target_sources(${target} PRIVATE ebur128/once.windows.c)
  elseif(TARGET Threads::Threads)
    target_sources(${target} PRIVATE ebur128/once.pthread.c)
    target_link_libraries(${target} PRIVATE Threads::Threads)
  else()
    target_sources(${target} PRIVATE ebur128/once.simple.c)
  endif()

  if(MATH_LIBRARY)
    target_link_libraries(${target} PRIVATE ${MATH_LIBRARY})
  endif()
endfunction()

ebur128_configure_library(ebur128)

if(ENABLE_FUZZER)
  target_compile_options(ebur128 PUBLIC "${FUZZER_FLAGS}")
  target_compile_definitions(ebur128 PRIVATE malloc=my_malloc calloc=my_calloc)
  target_link_libraries(ebur128 "${SANITIZER_FLAGS}")
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
  target_compile_features(ebur128-test PRIVATE cxx_std_20)
  target_link_libraries(ebur128-test PRIVATE ebur128::ebur128 ebur128::ebur128.test_signals)
  if(TARGET Threads::Threads)
    target_link_libraries(ebur128-test PRIVATE Threads::Threads)
  endif()
  if(EBUR128_TEST_PERFORMANCE)
    target_compile_definitions(ebur128-test PRIVATE EBUR128_TEST_PERFORMANCE)
  endif()
  ebur128_add_test(ebur128-test ebur128)

  add_executable(ebur128-test.true_peak ebur128-test.true_peak.cpp)
  target_compile_features(ebur128-test.true_peak PRIVATE cxx_std_20)
  target_link_libraries(ebur128-test.true_peak PRIVATE ebur128::ebur128 ebur128::ebur128.test_signals)
  if(TARGET Threads::Threads)
    target_link_libraries(ebur128-test.true_peak PRIVATE Threads::Threads)
  endif()
  ebur128_add_test(ebur128-test.true_peak ebur128)

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
    target_compile_features(ebur128-test.malloc_fail PRIVATE cxx_std_20)
    target_link_libraries(ebur128-test.malloc_fail PRIVATE ebur128::ebur128.malloc_fail)
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

cmake_minimum_required(VERSION 3.23)

foreach(required IN ITEMS
    SOURCE_DIR
    WORK_DIR
    EXPECTED_VERSION
    PKG_CONFIG_EXECUTABLE
    CONSUMER_SOURCE
    CMAKE_C_COMPILER)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "Missing required variable: ${required}")
  endif()
endforeach()

if(NOT DEFINED BUILD_SHARED_LIBS)
  message(FATAL_ERROR "Missing required variable: BUILD_SHARED_LIBS")
endif()

macro(run_command description)
  execute_process(
    COMMAND ${ARGN}
    RESULT_VARIABLE command_result
    OUTPUT_VARIABLE command_output
    ERROR_VARIABLE command_error)
  if(NOT command_result EQUAL 0)
    message(FATAL_ERROR
      "${description} failed (${command_result})\n"
      "${command_output}${command_error}")
  endif()
endmacro()

macro(pkg_config_query output_variable)
  execute_process(
    COMMAND
      "${CMAKE_COMMAND}" -E env
      "PKG_CONFIG_PATH=${pc_dir}"
      "PKG_CONFIG_LIBDIR=${pc_dir}"
      "${PKG_CONFIG_EXECUTABLE}" ${ARGN}
    RESULT_VARIABLE query_result
    OUTPUT_VARIABLE query_output
    ERROR_VARIABLE query_error
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT query_result EQUAL 0)
    message(FATAL_ERROR
      "pkg-config ${ARGN} failed (${query_result})\n"
      "${query_output}${query_error}")
  endif()
  set(${output_variable} "${query_output}")
endmacro()

if(BUILD_SHARED_LIBS)
  set(library_mode shared)
else()
  set(library_mode static)
endif()

message(STATUS "Testing ${library_mode} pkg-config installation in ${WORK_DIR}")

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

set(install_prefix "${WORK_DIR}/prefix")
set(nested_build "${WORK_DIR}/build")

set(configure_command
  "${CMAKE_COMMAND}"
  -S "${SOURCE_DIR}"
  -B "${nested_build}"
  "-DCMAKE_INSTALL_PREFIX=${install_prefix}"
  "-DBUILD_SHARED_LIBS=${BUILD_SHARED_LIBS}"
  -DBUILD_TESTING=OFF
  -DEBUR128_BUILD_EXAMPLES=OFF
  -DEBUR128_BUILD_BENCHMARKS=OFF
  "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}")

if(DEFINED CMAKE_GENERATOR AND NOT CMAKE_GENERATOR STREQUAL "")
  list(APPEND configure_command -G "${CMAKE_GENERATOR}")
endif()
if(DEFINED CMAKE_GENERATOR_PLATFORM AND
   NOT CMAKE_GENERATOR_PLATFORM STREQUAL "")
  list(APPEND configure_command -A "${CMAKE_GENERATOR_PLATFORM}")
endif()
if(DEFINED CMAKE_GENERATOR_TOOLSET AND
   NOT CMAKE_GENERATOR_TOOLSET STREQUAL "")
  list(APPEND configure_command -T "${CMAKE_GENERATOR_TOOLSET}")
endif()
if(DEFINED CMAKE_MAKE_PROGRAM AND NOT CMAKE_MAKE_PROGRAM STREQUAL "")
  list(APPEND configure_command "-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}")
endif()
if(DEFINED BUILD_CONFIG AND NOT BUILD_CONFIG STREQUAL "")
  list(APPEND configure_command "-DCMAKE_BUILD_TYPE=${BUILD_CONFIG}")
endif()

run_command("Configuring ${library_mode} build" ${configure_command})

set(build_command "${CMAKE_COMMAND}" --build "${nested_build}")
if(DEFINED BUILD_CONFIG AND NOT BUILD_CONFIG STREQUAL "")
  list(APPEND build_command --config "${BUILD_CONFIG}")
endif()
run_command("Building ${library_mode} library" ${build_command})

set(install_command "${CMAKE_COMMAND}" --install "${nested_build}")
if(DEFINED BUILD_CONFIG AND NOT BUILD_CONFIG STREQUAL "")
  list(APPEND install_command --config "${BUILD_CONFIG}")
endif()
run_command("Installing ${library_mode} library" ${install_command})

file(GLOB_RECURSE pc_files "${install_prefix}/libebur128.pc")
list(LENGTH pc_files pc_count)
if(NOT pc_count EQUAL 1)
  message(FATAL_ERROR
    "Expected exactly one installed libebur128.pc, found: ${pc_files}")
endif()
set(pc_file "${pc_files}")
get_filename_component(pc_dir "${pc_file}" DIRECTORY)

pkg_config_query(pkg_version --modversion libebur128)
if(NOT pkg_version STREQUAL EXPECTED_VERSION)
  message(FATAL_ERROR
    "Expected version ${EXPECTED_VERSION}, got ${pkg_version}")
endif()

pkg_config_query(pkg_prefix --variable=prefix libebur128)
if(NOT pkg_prefix STREQUAL install_prefix)
  message(FATAL_ERROR
    "Expected prefix ${install_prefix}, got ${pkg_prefix}")
endif()

pkg_config_query(pkg_includedir --variable=includedir libebur128)
pkg_config_query(pkg_libdir --variable=libdir libebur128)

string(FIND "${pkg_includedir}" "${install_prefix}/" includedir_position)
if(NOT includedir_position EQUAL 0)
  message(FATAL_ERROR
    "includedir ${pkg_includedir} is outside ${install_prefix}")
endif()
string(FIND "${pkg_libdir}" "${install_prefix}/" libdir_position)
if(NOT libdir_position EQUAL 0)
  message(FATAL_ERROR "libdir ${pkg_libdir} is outside ${install_prefix}")
endif()

if(NOT EXISTS "${pkg_includedir}/ebur128.h")
  message(FATAL_ERROR "Missing installed header: ${pkg_includedir}/ebur128.h")
endif()
if(NOT IS_DIRECTORY "${pkg_libdir}")
  message(FATAL_ERROR "Missing installed library directory: ${pkg_libdir}")
endif()

pkg_config_query(pkg_cflags --cflags libebur128)
string(FIND "${pkg_cflags}" "-I${pkg_includedir}" cflags_position)
if(cflags_position EQUAL -1)
  message(FATAL_ERROR
    "pkg-config --cflags does not include ${pkg_includedir}: ${pkg_cflags}")
endif()

pkg_config_query(pkg_libs --libs libebur128)
string(FIND "${pkg_libs}" "-L${pkg_libdir}" libdir_flag_position)
if(libdir_flag_position EQUAL -1)
  message(FATAL_ERROR
    "pkg-config --libs does not include ${pkg_libdir}: ${pkg_libs}")
endif()
string(FIND "${pkg_libs}" "-lebur128" library_flag_position)
if(library_flag_position EQUAL -1)
  message(FATAL_ERROR
    "pkg-config --libs does not include -lebur128: ${pkg_libs}")
endif()

if(BUILD_SHARED_LIBS)
  set(link_flags_text "${pkg_cflags} ${pkg_libs}")
else()
  pkg_config_query(pkg_static_libs --static --libs libebur128)
  set(link_flags_text "${pkg_cflags} ${pkg_static_libs}")
endif()
separate_arguments(link_flags NATIVE_COMMAND "${link_flags_text}")

set(consumer_executable "${WORK_DIR}/pkgconfig-consumer")
run_command(
  "Compiling ${library_mode} pkg-config consumer"
  "${CMAKE_C_COMPILER}"
  "${CONSUMER_SOURCE}"
  ${link_flags}
  -o "${consumer_executable}")

if(BUILD_SHARED_LIBS)
  set(run_env "LD_LIBRARY_PATH=${pkg_libdir}" "DYLD_LIBRARY_PATH=${pkg_libdir}")
  if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Haiku")
    execute_process(COMMAND findpaths -c : B_FIND_PATH_LIB_DIRECTORY OUTPUT_VARIABLE _libpath OUTPUT_STRIP_TRAILING_WHITESPACE)
    list(APPEND run_env "LIBRARY_PATH=${pkg_libdir}:${_libpath}")
  endif()
  set(run_command "${CMAKE_COMMAND}" -E env ${run_env} "${consumer_executable}")
else()
  set(run_command "${consumer_executable}")
endif()
run_command("Running ${library_mode} pkg-config consumer" ${run_command})

message(STATUS "pkg-config ${library_mode} test passed")

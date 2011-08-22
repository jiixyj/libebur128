find_package(PkgConfig)
pkg_check_modules(PC_SNDFILE QUIET sndfile)

find_path(SNDFILE_INCLUDE_DIR sndfile.h
          HINTS ${PC_SNDFILE_INCLUDEDIR} ${PC_SNDFILE_INCLUDE_DIRS})
find_library(SNDFILE_LIBRARY NAMES sndfile sndfile-1 libsndfile-1
             HINTS ${PC_SNDFILE_LIBDIR} ${PC_SNDFILE_LIBRARY_DIRS})

set(SNDFILE_LIBRARIES ${SNDFILE_LIBRARY})
set(SNDFILE_INCLUDE_DIRS ${SNDFILE_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SNDFILE DEFAULT_MSG SNDFILE_LIBRARY SNDFILE_INCLUDE_DIR)
mark_as_advanced(SNDFILE_INCLUDE_DIR SNDFILE_LIBRARY)

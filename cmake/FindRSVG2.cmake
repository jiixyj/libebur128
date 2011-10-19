find_package(PkgConfig)
pkg_check_modules(PC_RSVG2 QUIET librsvg-2.0)

find_path(RSVG2_INCLUDE_DIR librsvg/rsvg.h
          HINTS ${PC_RSVG2_INCLUDEDIR} ${PC_RSVG2_INCLUDE_DIRS}
          PATH_SUFFIXES librsvg-2.0 librsvg-2)
find_library(RSVG2_LIBRARY NAMES rsvg rsvg-2
             HINTS ${PC_RSVG2_LIBDIR} ${PC_RSVG2_LIBRARY_DIRS})

set(RSVG2_LIBRARIES ${RSVG2_LIBRARY})
set(RSVG2_INCLUDE_DIRS ${RSVG2_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(RSVG2 DEFAULT_MSG RSVG2_LIBRARY RSVG2_INCLUDE_DIR)
mark_as_advanced(RSVG2_INCLUDE_DIR RSVG2_LIBRARY)

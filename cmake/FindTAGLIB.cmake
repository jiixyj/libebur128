find_package(PkgConfig)
pkg_check_modules(PC_TAGLIB QUIET taglib)

find_path(TAGLIB_INCLUDE_DIR taglib.h PATH_SUFFIXES taglib
          HINTS ${PC_TAGLIB_INCLUDEDIR} ${PC_TAGLIB_INCLUDE_DIRS})
find_library(TAGLIB_LIBRARY tag
             HINTS ${PC_TAGLIB_LIBDIR} ${PC_TAGLIB_LIBRARY_DIRS})

set(TAGLIB_LIBRARIES ${TAGLIB_LIBRARY})
set(TAGLIB_INCLUDE_DIRS ${TAGLIB_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(TAGLIB DEFAULT_MSG TAGLIB_LIBRARY TAGLIB_INCLUDE_DIR)
mark_as_advanced(TAGLIB_INCLUDE_DIR TAGLIB_LIBRARY)

find_package(PkgConfig)
pkg_check_modules(PC_LIBAVFORMAT QUIET libavformat)

find_path(AVFORMAT_INCLUDE_DIR libavformat/avformat.h
          HINTS ${PC_LIBAVFORMAT_INCLUDEDIR} ${PC_LIBAVFORMAT_INCLUDE_DIRS})

find_library(AVFORMAT_LIBRARY NAMES avformat avformat-52 avformat-53
             HINTS ${PC_LIBAVFORMAT_LIBDIR} ${PC_LIBAVFORMAT_LIBRARY_DIRS})

set(AVFORMAT_LIBRARIES ${AVFORMAT_LIBRARY})
set(AVFORMAT_INCLUDE_DIRS ${AVFORMAT_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(AVFORMAT DEFAULT_MSG
                 AVFORMAT_LIBRARY AVFORMAT_INCLUDE_DIR)
mark_as_advanced(AVFORMAT_LIBRARY AVFORMAT_INCLUDE_DIR)

find_package(PkgConfig)
pkg_check_modules(PC_LIBAVFORMAT QUIET libavformat)
pkg_check_modules(PC_LIBAVCODEC QUIET libavcodec)
pkg_check_modules(PC_LIBAVUTIL QUIET libavutil)

find_path(AVFORMAT_INCLUDE_DIR libavformat/avformat.h
          HINTS ${PC_LIBAVFORMAT_INCLUDEDIR} ${PC_LIBAVFORMAT_INCLUDE_DIRS})

find_library(AVFORMAT_LIBRARY NAMES avformat avformat-52 avformat-53
             HINTS ${PC_LIBAVFORMAT_LIBDIR} ${PC_LIBAVFORMAT_LIBRARY_DIRS})

find_library(AVCODEC_LIBRARY NAMES avcodec avcodec-52 avcodec-53
             HINTS ${PC_LIBAVCODEC_LIBDIR} ${PC_LIBAVCODEC_LIBRARY_DIRS})

find_library(AVUTIL_LIBRARY NAMES avutil avutil-50 avutil-51
             HINTS ${PC_LIBAVUTIL_LIBDIR} ${PC_LIBAVUTIL_LIBRARY_DIRS})

set(AVFORMAT_LIBRARIES ${AVFORMAT_LIBRARY} ${AVCODEC_LIBRARY} ${AVUTIL_LIBRARY})
set(AVFORMAT_INCLUDE_DIRS ${AVFORMAT_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(AVFORMAT DEFAULT_MSG
                 AVFORMAT_LIBRARY AVFORMAT_INCLUDE_DIR)
mark_as_advanced(AVFORMAT_LIBRARY AVFORMAT_INCLUDE_DIR)

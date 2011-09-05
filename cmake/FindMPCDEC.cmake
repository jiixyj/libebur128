find_path(MPCDEC_INCLUDE_DIR mpc/mpcdec.h)
find_library(MPCDEC_LIBRARY NAMES mpcdec mpcdec_static)

set(MPCDEC_LIBRARIES ${MPCDEC_LIBRARY})
set(MPCDEC_INCLUDE_DIRS ${MPCDEC_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MPCDEC DEFAULT_MSG MPCDEC_LIBRARY MPCDEC_INCLUDE_DIR)
mark_as_advanced(MPCDEC_INCLUDE_DIR MPCDEC_LIBRARY)

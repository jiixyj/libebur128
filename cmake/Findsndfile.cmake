# CMake config for systems with a CMake build of sndfile (i.e., windows with vcpkg)
find_package(LibSndFile QUIET CONFIG)

set(SNDFILE_CFLAGS)

if(LibSndFile_FOUND)
    set(SNDFILE_INCLUDE_DIRS)
    if(TARGET sndfile-shared AND (BUILD_SHARED_LIBS OR NOT TARGET sndfile-static))
        set(SNDFILE_LIBRARIES sndfile-shared)
        return()
    elseif(TARGET sndfile-static)
        set(SNDFILE_LIBRARIES sndfile-static)
        return()
    endif()
endif()

# pkgconfig for linux and other systems with pkgconfig
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_SNDFILE QUIET sndfile)
    if(PC_SNDFILE_FOUND)
        set(SNDFILE_INCLUDE_DIRS ${PC_SNDFILE_INCLUDEDIR} ${PC_SNDFILE_INCLUDEDIRS})
        set(SNDFILE_LIBRARIES)
        foreach(_LIB ${PC_SNDFILE_LIBRARIES})
            find_library(SNDFILE_${_LIB}_LIBRARY ${_LIB} HINTS ${PC_SNDFILE_LIBDIR} ${PC_SNDFILE_LIBRARY_DIRS})
            list(APPEND SNDFILE_LIBRARIES ${SNDFILE_${_LIB}_LIBRARY})
            mark_as_advanced(SNDFILE_${_LIB}_LIBRARY)
        endforeach()
        foreach(_FLAG ${PC_SNDFILE_CFLAGS_OTHER})
            set(SNDFILE_CFLAGS "${SNDFILE_CFLAGS} ${_FLAG}")
        endforeach()
        return()
    endif()
endif()

# default find script may fail due to missing dependencies
find_path(SNDFILE_INCLUDE_DIR sndfile.h)
find_library(SNDFILE_LIBRARY NAMES sndfile sndfile-1 libsndfile libsndfile-1)
find_package_handle_standard_args(sndfile DEFAULT_MSG SNDFILE_LIBRARY SNDFILE_INCLUDE_DIR)
mark_as_advanced(SNDFILE_INCLUDE_DIR SNDFILE_LIBRARY)

set(SNDFILE_INCLUDE_DIRS ${SNDFILE_INCLUDE_DIR})
set(SNDFILE_LIBRARIES ${SNDFILE_LIBRARY})

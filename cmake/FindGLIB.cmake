find_package(PkgConfig)
pkg_check_modules(PC_GLIB QUIET glib-2.0)
pkg_check_modules(PC_GTHREAD QUIET gthread-2.0)
pkg_check_modules(PC_GMODULE QUIET gmodule-2.0)
pkg_check_modules(PC_GOBJECT QUIET gobject-2.0)

set(GLIB_CFLAGS)
foreach(arg ${PC_GLIB_CFLAGS})
    if (NOT "${arg}" MATCHES "-I.*")
        set(GLIB_CFLAGS "${GLIB_CFLAGS} ${arg}")
    endif()
endforeach()

find_path(GLIB_INCLUDE_DIR glib.h PATH_SUFFIXES glib-2.0
          HINTS ${PC_GLIB_INCLUDEDIR} ${PC_GLIB_INCLUDE_DIRS})
find_path(GLIB_LIB_INCLUDE_DIR glibconfig.h PATH_SUFFIXES ../lib/glib-2.0/include
          HINTS ${PC_GLIB_INCLUDEDIR} ${PC_GLIB_INCLUDE_DIRS})
find_library(GLIB_LIBRARY NAMES glib-2.0 glib-2.0-0
             HINTS ${PC_GLIB_LIBDIR} ${PC_GLIB_LIBRARY_DIRS})
find_library(GTHREAD_LIBRARY NAMES gthread-2.0 gthread-2.0-0
             HINTS ${PC_GTHREAD_LIBDIR} ${PC_GTHREAD_LIBRARY_DIRS})
find_library(GMODULE_LIBRARY NAMES gmodule-2.0 gmodule-2.0-0
             HINTS ${PC_GMODULE_LIBDIR} ${PC_GMODULE_LIBRARY_DIRS})
find_library(GOBJECT_LIBRARY NAMES gobject-2.0 gobject-2.0-0
             HINTS ${PC_GOBJECT_LIBDIR} ${PC_GOBJECT_LIBRARY_DIRS})

set(GLIB_LIBRARIES ${GLIB_LIBRARY} ${GTHREAD_LIBRARY} ${GMODULE_LIBRARY} ${GOBJECT_LIBRARY})
set(GLIB_INCLUDE_DIRS ${GLIB_INCLUDE_DIR} ${GLIB_LIB_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GLIB DEFAULT_MSG GLIB_LIBRARY GTHREAD_LIBRARY GMODULE_LIBRARY GOBJECT_LIBRARY GLIB_INCLUDE_DIR GLIB_LIB_INCLUDE_DIR)
mark_as_advanced(GLIB_LIBRARY GTHREAD_LIBRARY GMODULE_LIBRARY GOBJECT_LIBRARY GLIB_INCLUDE_DIR GLIB_LIB_INCLUDE_DIR)

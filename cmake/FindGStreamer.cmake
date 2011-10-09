# - Try to find GStreamer
# Once done this will define
#
#  GSTREAMER_FOUND - system has GStreamer
#  GSTREAMER_INCLUDE_DIR - the GStreamer include directory
#  GSTREAMER_LIBRARIES - the libraries needed to use GStreamer
#  GSTREAMER_DEFINITIONS - Compiler switches required for using GStreamer

# Copyright (c) 2006, Tim Beaulen <tbscope@gmail.com>
# Copyright (c) 2008, Helio Chissini de Castro <helio@kde.org>
# Copyright (c) 2010, Ni Hui <shuizhuyuanluo@126.com>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if(GSTREAMER_INCLUDE_DIR AND GSTREAMER_LIBRARIES)
   # in cache already
   set(GStreamer_FIND_QUIETLY TRUE)
endif(GSTREAMER_INCLUDE_DIR AND GSTREAMER_LIBRARIES)

if(NOT WIN32)
   find_package(PkgConfig)
   PKG_CHECK_MODULES(PKG_GSTREAMER gstreamer-0.10)
   set(GSTREAMER_VERSION ${PKG_GSTREAMER_VERSION})
   set(GSTREAMER_DEFINITIONS ${PKG_GSTREAMER_CFLAGS})
endif(NOT WIN32)

find_path(GSTREAMER_INCLUDE_DIR gst/gst.h
   PATHS
   ${PKG_GSTREAMER_INCLUDE_DIRS}
   PATH_SUFFIXES gstreamer-0.10
)

find_library(GSTREAMER_LIBRARY NAMES gstreamer-0.10
   PATHS ${PKG_GSTREAMER_LIBRARY_DIRS}
) 

find_library(GSTAPP_LIBRARY NAMES gstapp-0.10
   PATHS ${PKG_GSTREAMER_LIBRARY_DIRS}
) 

find_library(GSTBASE_LIBRARY NAMES gstbase-0.10
   PATHS ${PKG_GSTREAMER_LIBRARY_DIRS}
) 

find_library(GSTAUDIO_LIBRARY NAMES gstaudio-0.10
   PATHS ${PKG_GSTREAMER_LIBRARY_DIRS}
) 

set(GSTREAMER_LIBRARIES ${GSTREAMER_LIBRARY} ${GSTAPP_LIBRARY} ${GSTBASE_LIBRARY} ${GSTAUDIO_LIBRARY})

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(GStreamer DEFAULT_MSG GSTREAMER_LIBRARIES GSTREAMER_INCLUDE_DIR)

mark_as_advanced(GSTREAMER_INCLUDE_DIR GSTREAMER_LIBRARIES)

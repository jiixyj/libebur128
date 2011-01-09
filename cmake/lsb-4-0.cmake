# Based on http://www.cmake.org/Wiki/CmakeMingw

# the name of the target operating system
SET(CMAKE_SYSTEM_NAME Linux)

# which compilers to use for C and C++
SET(CMAKE_C_COMPILER /opt/lsb/bin/lsbcc)
SET(CMAKE_CXX_COMPILER /opt/lsb/bin/lsbc++)

# here is the target environment located
SET(CMAKE_FIND_ROOT_PATH /opt/lsb/)

# adjust the default behaviour of the FIND_XXX() commands:
# search headers and libraries in the target environment, search
# programs in the host environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

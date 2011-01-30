# Based on http://www.cmake.org/Wiki/CmakeMingw

# the name of the target operating system
SET(CMAKE_SYSTEM_NAME Windows)

# which compilers to use for C and C++
SET(CMAKE_C_COMPILER wine-gcc)
SET(CMAKE_CXX_COMPILER wine-g++)

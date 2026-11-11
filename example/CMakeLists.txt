find_package(PkgConfig REQUIRED)
pkg_check_modules(SNDFILE REQUIRED IMPORTED_TARGET sndfile)

add_executable(minimal-example minimal-example.c)
target_link_libraries(minimal-example PRIVATE ebur128 PkgConfig::SNDFILE)

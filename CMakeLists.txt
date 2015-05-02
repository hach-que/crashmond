cmake_minimum_required(VERSION 2.6)
project(crashmond)

find_package(PkgConfig)
pkg_check_modules(SYSTEMD_JOURNAL libsystemd-journal)
pkg_check_modules(CURL libcurl)

SET(CMAKE_C_COMPILER "/usr/bin/clang")
SET(CMAKE_CXX_COMPILER "/usr/bin/clang++")

# We have to turn off some warnings due to libancillary
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror -Weverything -Wno-cast-align -Wno-reserved-id-macro -Wno-sign-conversion -Wno-sign-compare -Wno-shorten-64-to-32 -Wno-gnu-variable-sized-type-not-at-end -Wno-unused-macros -Wno-padded -Wno-disabled-macro-expansion")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -Weverything -Wno-cast-align -Wno-reserved-id-macro -Wno-sign-conversion -Wno-sign-compare -Wno-shorten-64-to-32 -Wno-gnu-variable-sized-type-not-at-end -Wno-unused-macros -Wno-padded -Wno-disabled-macro-expansion")

include_directories(${SYSTEMD_JOURNAL_INCLUDEDIR} ${CURL_INCLUDEDIR})
add_executable(crashmond
  main.c
  daemon.c
  handler.c
  submit.c
  libancillary/fd_recv.c
  libancillary/fd_send.c)
target_link_libraries(crashmond ${SYSTEMD_JOURNAL_LIBRARIES} ${CURL_LIBRARIES})

install(TARGETS crashmond RUNTIME DESTINATION bin)

cmake_minimum_required(VERSION 3.13)
project(doom VERSION 1.10 LANGUAGES C)

add_executable(doom)
target_compile_options(doom PUBLIC -std=gnu17 -Wall -DNORMALUNIX -DLINUX)

add_subdirectory(src)

target_link_libraries(doom PRIVATE Xext X11 nsl m)

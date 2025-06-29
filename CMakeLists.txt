cmake_minimum_required(VERSION 3.13)
project(doom VERSION 1.10 LANGUAGES C)

add_executable(doom)
target_compile_options(doom PUBLIC -std=gnu17 -DNORMALUNIX -DLINUX -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast)

add_subdirectory(src)

target_link_libraries(doom PRIVATE m SDL3 SDL3_mixer jansson)

add_custom_command(
    OUTPUT portdata.wad
    COMMAND wadscript
    ARGS ${CMAKE_SOURCE_DIR}/buildwad.wscript
        ${CMAKE_SOURCE_DIR}/wad/
    DEPENDS
        wad/SBARDEF)

add_custom_target(port_data_wad ALL
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/portdata.wad")

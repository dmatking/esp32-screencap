# screencap.cmake — include this in your sim/CMakeLists.txt, then call
# screencap_add_sim() to get a fully-linked simulator target.
#
# Usage:
#   include(path/to/screencap/cmake/screencap.cmake)
#
#   screencap_add_sim(my_sim
#       SOURCES  main_sim.c ../main/foo.c ../main/bar.c
#       INCLUDES ../main                        # extra include dirs (optional)
#   )

cmake_minimum_required(VERSION 3.16)

# Locate this file's directory so relative paths to screencap stay correct
# even when the caller's CMakeLists.txt is in a different directory.
get_filename_component(_SCREENCAP_DIR "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
get_filename_component(_SCREENCAP_ROOT "${_SCREENCAP_DIR}" DIRECTORY)

find_package(SDL2 REQUIRED)

function(screencap_add_sim TARGET_NAME)
    cmake_parse_arguments(ARG "" "" "SOURCES;INCLUDES" ${ARGN})

    add_executable(${TARGET_NAME}
        ${ARG_SOURCES}
        ${_SCREENCAP_ROOT}/src/screencap_sdl.c
        ${_SCREENCAP_ROOT}/src/screencap_png.c
    )

    # esp_stubs/ shadows ESP-IDF headers; list it first so it wins
    target_include_directories(${TARGET_NAME} PRIVATE
        ${_SCREENCAP_ROOT}/include/esp_stubs   # driver/, freertos/ live here
        ${_SCREENCAP_ROOT}/include             # screencap.h
        ${ARG_INCLUDES}
    )

    target_link_libraries(${TARGET_NAME} PRIVATE SDL2::SDL2)

    target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra)
    # SDL_MAIN_HANDLED is defined in screencap_sdl.c — no compile definition needed

    message(STATUS "[screencap] target '${TARGET_NAME}' configured")
    message(STATUS "[screencap]   P = save screenshot_NNNN.png")
    message(STATUS "[screencap]   Esc / close = quit")
    message(STATUS "[screencap]   ./${TARGET_NAME} --screenshot out.png [--frames N]")
endfunction()

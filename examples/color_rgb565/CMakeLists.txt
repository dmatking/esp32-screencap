cmake_minimum_required(VERSION 3.16)
project(color_rgb565_sim C)

include(../../cmake/screencap.cmake)

screencap_add_sim(color_rgb565_sim
    SOURCES
        main_sim.c
        display_sim.c
        display_common.c
    INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(color_rgb565_sim PRIVATE m)

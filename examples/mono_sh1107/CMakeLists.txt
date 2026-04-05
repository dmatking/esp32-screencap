cmake_minimum_required(VERSION 3.16)
project(mono_sh1107_sim C)

include(../../cmake/screencap.cmake)

screencap_add_sim(mono_sh1107_sim
    SOURCES
        main_sim.c
        sh1107_sim.c
        sh1107_common.c
    INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mono_sh1107_sim PRIVATE m)   # math for demo

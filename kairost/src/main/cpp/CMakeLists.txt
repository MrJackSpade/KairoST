cmake_minimum_required(VERSION 3.22.1)
project(kairost LANGUAGES C CXX)

get_filename_component(KAIROST_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
include("${KAIROST_ROOT}/shared/cmake/KairoMachine.cmake")

# The Atari ST on Hatari, driven one frame at a time (core/).
add_subdirectory("${KAIROST_ROOT}/core" core)

kairo_machine_library(kairost st_core.cpp)
target_link_libraries(kairost PRIVATE kairost_core)
if(NOT ANDROID)
    # The shared PC boot test loads Hatari as the app does, a library beside it (hatari_library.cpp).
    add_dependencies(kairost kairost_hatari)
    add_custom_command(TARGET kairost POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:kairost_hatari> $<TARGET_FILE_DIR:kairost>)
endif()

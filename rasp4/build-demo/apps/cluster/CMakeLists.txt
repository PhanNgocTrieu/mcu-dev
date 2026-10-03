cmake_minimum_required(VERSION 3.16)
project(hupi-cluster VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(HUPI_HOST_SDL "Link the host SDL2 .so without pkg-config (dev laptop)" OFF)

get_filename_component(HUPI_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
if(NOT TARGET hupi_wire)
    add_subdirectory(${HUPI_ROOT}/modules/common ${CMAKE_CURRENT_BINARY_DIR}/hupi-common)
endif()

if(NOT TARGET hupi_sdl)
    add_library(hupi_sdl INTERFACE)
    if(HUPI_HOST_SDL)
        target_include_directories(hupi_sdl INTERFACE ${HUPI_ROOT}/third_party/sdl2/include)
        target_link_libraries(hupi_sdl INTERFACE /usr/lib/x86_64-linux-gnu/libSDL2-2.0.so.0 m)
    else()
        find_package(SDL2 REQUIRED)
        target_link_libraries(hupi_sdl INTERFACE SDL2::SDL2)
    endif()
endif()

add_executable(hupi-cluster main.c ../common/ui.c)
target_include_directories(hupi-cluster PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../common ${HUPI_ROOT}/third_party)
target_link_libraries(hupi-cluster PRIVATE hupi_wire hupi_sdl m)
target_compile_options(hupi-cluster PRIVATE -Wall -Wextra -Werror -Wno-format-truncation)
set_source_files_properties(../common/ui.c PROPERTIES COMPILE_OPTIONS "-Wno-unused-function;-Wno-missing-field-initializers")

include(GNUInstallDirs)
install(TARGETS hupi-cluster RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(PROGRAMS hupi-ui DESTINATION ${CMAKE_INSTALL_BINDIR})

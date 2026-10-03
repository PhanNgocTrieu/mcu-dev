cmake_minimum_required(VERSION 3.16)
project(libhu-carplay VERSION 1.0.0 LANGUAGES C)

option(HUPI_WITH_MFI "Link proprietary libiap2 / libhu-mfi" OFF)
option(HUPI_MEDIA_H264_STUB "Emit hardcoded H.264 AUs from libhu-carplay" ON)

get_filename_component(MODULES_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
if(NOT TARGET hupi_wire)
    add_subdirectory(${MODULES_ROOT}/common ${CMAKE_CURRENT_BINARY_DIR}/hupi-common)
endif()

add_library(hu_carplay STATIC src/hu_carplay.c)
target_include_directories(hu_carplay PUBLIC include)
target_link_libraries(hu_carplay PUBLIC hupi_wire)
target_compile_options(hu_carplay PRIVATE -Wall -Wextra -Werror -Wno-format-truncation)
target_compile_definitions(hu_carplay PUBLIC
    HUPI_WITH_MFI=$<BOOL:${HUPI_WITH_MFI}>
    HUPI_MEDIA_H264_STUB=$<BOOL:${HUPI_MEDIA_H264_STUB}>)

include(GNUInstallDirs)
install(TARGETS hu_carplay ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES include/hu_carplay.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

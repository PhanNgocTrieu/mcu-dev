cmake_minimum_required(VERSION 3.16)
project(libhu-aa VERSION 1.0.0 LANGUAGES C CXX)

option(HUPI_WITH_AASDK "Link the real f1xpl/aasdk stack" OFF)

get_filename_component(MODULES_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
if(NOT TARGET hupi_wire)
    add_subdirectory(${MODULES_ROOT}/common ${CMAKE_CURRENT_BINARY_DIR}/hupi-common)
endif()

add_library(hu_aa STATIC src/hu_aa.c)
target_include_directories(hu_aa PUBLIC include)
target_link_libraries(hu_aa PUBLIC hupi_wire)
target_compile_options(hu_aa PRIVATE -Wall -Wextra -Werror -Wno-format-truncation)
target_compile_definitions(hu_aa PUBLIC HUPI_WITH_AASDK=$<BOOL:${HUPI_WITH_AASDK}>)

if(HUPI_WITH_AASDK)
    message(STATUS "libhu-aa: building against AASDK")
    # Expected: AASDK_ROOT points at a checked-out f1xpl/aasdk tree.
    if(NOT AASDK_ROOT)
        set(AASDK_ROOT "${CMAKE_SOURCE_DIR}/third_party/aasdk")
    endif()
    if(EXISTS "${AASDK_ROOT}/CMakeLists.txt")
        add_subdirectory(${AASDK_ROOT} ${CMAKE_CURRENT_BINARY_DIR}/aasdk EXCLUDE_FROM_ALL)
        target_link_libraries(hu_aa PUBLIC aasdk)
    else()
        message(WARNING "HUPI_WITH_AASDK=ON but AASDK_ROOT missing; shim stays active")
        target_compile_definitions(hu_aa PUBLIC HUPI_WITH_AASDK=0)
    endif()
endif()

include(GNUInstallDirs)
install(TARGETS hu_aa ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES include/hu_aa.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
